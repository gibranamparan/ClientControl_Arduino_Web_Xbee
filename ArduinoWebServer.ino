#include <Servo.h> 
#include <SPI.h>
#include <Ethernet.h>

//Declaracion de servidor web
EthernetServer server(80);

//Almacenamiento de datos de la URL
String valParams[4];
String nameParams[4];
String request="";
String url="";
String action="";

//Pulso y Timers para obtener medicion de gotas por minuto (GPMM)
unsigned long time1;
unsigned long time2;
int gota = 4;//Pin donde se recibe el pulso de goteo

//Datos para comunicacion con aplicacion
int GPM = 0;
int GPMM = 0;
int servoDegrees = 0;
int autoControl = 0;
boolean flag = true; //Bandera marca inicio y fin de señal de una gota

/*PINS INDICADORES*/
int pinError = 8;
int pinSuccess = 6;
bool isErrorOn = false;
bool isSuccessOn = false;

void setup() {
  pinMode(pinError,OUTPUT);
  pinMode(pinSuccess,OUTPUT);
  
  //Se indica incializacion de servidor (error = 1, succes = 1)
  digitalWrite(pinError, HIGH);
  digitalWrite(pinSuccess, HIGH);
  
  pinMode(gota,INPUT);//Se indica el pin donde se recibira la señal de goteo del PLC
  time1 = millis();//Toma tiempo inicial de referencia para medir goteo
  Serial.begin(9600); //Inicia conexion usb
  byte mac[] = { 0xDE, 0xAD, 0xBE, 0xEF, 0xFE, 0xED };
  
  //Configuracion LAGUNA DEL MAR
  /*byte ip[] = { 10, 10, 100, 34};
  byte dns[] = { 10, 10, 100, 253};
  byte gateway[] = { 10, 10, 100, 253};
  byte subnet[] = { 255, 255, 255, 0};
  Serial.println("STATIC IP TEST");
  Ethernet.begin(mac, ip, dns, gateway, subnet);//Inicia conexion con ethernet shield*/
  
  /*Conexion de Server a la red obteniendo IP por DHCP*/
  Serial.println("DYN IP Connecting");//Imprime ip del servidor ethernet
  Ethernet.begin(mac);//Inicia conexion con ethernet shield*/

  /*Serial.println("STATIC IP TEST");//Imprime ip del servidor ethernet
  Ethernet.begin(mac, ip);//Inicia conexion con ethernet shield*/
  
  server.begin();//inicia server ethernet
  String ipServer = ipAdressToString(Ethernet.localIP());
  if(ipServer == "0.0.0.0"){
    //Se indica error para adquirir IP (error = 1, succes = 0)
    digitalWrite(pinError, HIGH);
    digitalWrite(pinSuccess, LOW);
  }else{
    //Se indica fin de inicializacion (error = 0, succes = 0)
    digitalWrite(pinError, LOW);
    digitalWrite(pinSuccess, LOW);
  }
  Serial.println(ipServer);//Imprime ip del servidor ethernet
}

void loop() {
  //Reiniciar indicadores LED
  if(isSuccessOn){
    digitalWrite(pinSuccess, LOW);
    isSuccessOn = false;
  }
  
  EthernetClient client = server.available();
  // an http request ends with a blank line
  boolean currentLineIsBlank = true;
  
  if (client) { //Si el cliente se conecto
    action = "";
    while (client.connected()) { //Y mientras se encuentre conectado
      if (client.available()) { //Si hay un dato entrando por el socket
        //Se lee el request caracter por caracter
        char c = client.read();
        request+=c;
        
        if (c == '\n' && currentLineIsBlank) {//Si Se tomo todo el request del explorador
          url=getURL();//Se aisla la url
          separateURL(url);//Se toma la accion y los parametros
          action=getValByName("action");
          
          client.println("HTTP/1.1 200 OK");
          client.println("Content-Type: application/json;charset=utf-8");
          client.println();
          //Recibe datos para girar
          if(action=="mover"){
            autoControl = getValByName("autoControl").toInt();
            if(autoControl==1)
              GPM = getValByName("GPM").toInt();
            else
              servoDegrees = getValByName("servoDegrees").toInt();
          }
          
          String dataToSend = "{GPM:"+String(GPM)+",servoDegrees:"+String(servoDegrees)+",GPMM:"+String(GPMM)+",autoControl:"+String(autoControl)+"}";
          //Serial.println(dataToSend);
          client.println(dataToSend);
          
          //Se indica que se respondio a un cliente con informacion
          digitalWrite(pinSuccess, HIGH);
          isSuccessOn = true;
                    
          break;         
        }//Fin de acciones al tomar URL
        
        if (c == '\n') {
          // you're starting a new line
          currentLineIsBlank = true;
        } else if (c != '\r') {
          // you've gotten a character on the current line
          currentLineIsBlank = false;
        }
      }//Fin Si hay dato en socket
    }//Fin mientras esta conectado
    request="";//Se resetea el request

    // Tiempo para que el browser reciba el response
    delay(1);
    // Cierra conexion
    client.stop();
  }//Fin de conexion del cliente
  
  /***ALGORITMO PARA MEDICION DE GOTEO***/
  //Se detecta la caida de una gota, mientras la señal sea alta, la bandera flag se levanta
  // y el hilo no entrara a este bloque, esto evita contar la misma gota mas de una vez.
  if(digitalRead(gota) == HIGH && flag){
    flag = false;
    
    time2 = millis();//Se toma el tiempo actual como un nuevo tiempo
    if(time2>time1){//Evita divisiones entre cero o numeros negativos
      //SE OBTIENE LA MEDICION DE GPM,
      //Se mide cuantas veces el intervalo de tiempo entre gota y gota cabe dentro de 60mil ms. (1min)
      GPMM = 60000 / (time2 - time1 - 1); //La resta de tiempos representa el intervalo entre gota y gota
    }
    time1=time2;//Se guarda el tiempo anterior
  }
  
  //Solamente se baja la bandera cuando se detecte una transicion
  //que indica que la gota ha dejado de pasar por el sensor,
  //lo que permitira sensar una nueva gota
  if(digitalRead(gota) == LOW && !flag){
    flag = true;
  }

  //Enviando al receptor XBEE para girar el motor de la valvula
//  sendStartOrEnd(true);
  sendIntByBytes(GPM);
  sendIntByBytes(GPMM);
  sendIntByBytes(servoDegrees);
  sendIntByBytes(autoControl);
//  sendStartOrEnd(false);
  
}//Fin loop

//Para optimizar la velocidad de la comunicacion al XBEE receptor, 
//se envia el numero de 16bits en 2 bytes separados
void sendIntByBytes(int number){
  uint16_t num = number;
  uint16_t mask   = B11111111;          // 0000 0000 1111 1111
  uint8_t first_half   = number >> 8;   // >>>> >>>> 0001 0110
  uint8_t sencond_half = number & mask; // ____ ____ 0100 0111
  
  Serial.write(first_half);
  Serial.write(sencond_half);
}

//Send start or end (true or false) to mark the beginnig or end of transmission
void sendStartOrEnd(bool startEnd){
  char charToSend=startEnd?'{':'}';
  Serial.write(charToSend);
}

//Parsea la solicitud HTTP para tomar la URL solicitada al servidor
String getURL(){
  String startStr="GET /";
  String endStr="HTTP/";
  int startIdx = request.indexOf(startStr)+startStr.length();
  int endIdx = request.indexOf("HTTP/")-1;
  url = request.substring(startIdx,endIdx);
  return url;
}

//Cuenta la cantidad de Get params contando el char '='
int numberOfVarsInUrl(String url){
  int noVars=0;
  for(int c=0;c<url.length();c++)
    if(url[c]=='=')
      noVars++;
  
  return noVars;
}

//Regresa un arreglo de Strings con action y params separados
void separateURL(String url){
  String action="";
  int c=1;
  //Si es URL con parametros GET
  if(url.indexOf('?')>=0){
    action = url.substring(0,url.indexOf('?'));
    String params = url.substring(url.indexOf('?')+1);
    int noVars=numberOfVarsInUrl(url);
    String val="";
    String nameParam="";
    while(true){
      //Processing get parameters
      val = params.substring(0,params.indexOf('&')).substring(params.indexOf('=')+1);
      nameParam = params.substring(0,params.indexOf('&')).substring(0,params.indexOf('='));
      valParams[c] = val;
      nameParams[c] = nameParam;
      c++;
      
      if(params.indexOf('&')!=-1)
        params = params.substring(params.indexOf('&')+1);
      else
        break;
    }
  }
  valParams[0] = action;
  nameParams[0] = "action";
}

//Dada la cadena de parametros get del la URL, 
//se aisla el valor de la variable indicada por el argumento valName
String getValByName(String valName){
  String result="";
  for(int c=0;c<4;c++){
    if(nameParams[c]==valName){
      result = valParams[c];
      break;
    }
  }
  return result;
}

//Convierte la IP obtenida por la inicializacion del servidor en valor en string
String ipAdressToString(IPAddress address)
{
   return String(address[0]) + "." + 
        String(address[1]) + "." + 
        String(address[2]) + "." + 
        String(address[3]);
}
