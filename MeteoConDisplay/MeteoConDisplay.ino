#ifdef ESP8266
#include <ESP8266WiFi.h>
#else
#include <WiFi.h>
#endif
#include <stdio.h>
#include <ArduinoJson.h> //configuratore: https://arduinojson.org/v6/assistant/#/step1
#include "Utilities.h" //file proprietario
#include <RTClib.h> //Adafruit RTClib per DS3231
#include "EasyNextionLibrary.h"  //EasyNextionLibrary

//NEXTION. CREAZIONE DELL'OGGETTO PER LA COMUNICAZIONE CON HMI NEXTION
//PASSARE IN INGRESSO LA SERIALE DESIDERATA
EasyNex myNex(Serial); 
//CREAZIONE DEL OGGETTO DS3231 PER LEGGERE LA TEMPERATURA E DATA-ORA ATTUALI
RTC_DS3231 rtc;
//INDICA SE L'INIZIALIZZAZIONE E LA COMUNICAZIONE CON DS3231 HA AVUTO SUCCESSO
bool rtcOK=false;

//Inizializza il WiFi
WiFiClient client;
const char* ssid = "Network";                          
const char* password = "Psw";  

//ISTANZA DEL OGGETTO TIMER (VEDI UTILITIES.H) PER CAMPIONAMENTO DEI VALORI SUL METEO
TimerC* httpGetTimer = new TimerC();
//Ogni httpGetInterval viene fatta la Get per le info sul METEO
const unsigned long httpGetInterval = 30L * 1000L;  //=30000ms = 30s in millisecondi
//API-KEY OTTENUTA DAL SERVIZIO: https://openweathermap.org/api
const String API_KEY = "1ea6759cf4475050fc89dde08ec30abf";
//NOME E COUNTRY CODE DELLA CITTA DA MONITORARE
String CITY_NAME = "milano";
const String COUNTRY_CODE = "it";

//DEPRECATO
//const String GET_URI_UPDATE = "GET /data/2.5/weather?q="+CITY_NAME+","+COUNTRY_CODE+"&units=metric&APPID="+API_KEY+" HTTP/1.1";
//const String GET_URI_LATLON_BY_NAME = "GET /geo/1.0/direct?q="+CITY_NAME+","+COUNTRY_CODE+"&limit=1&appid="+API_KEY+" HTTP/1.1";//solo la prima città limit=1;

bool first_scan=true;

//ISTANZA DELL'OGGETTO Info (vedi Utilities.h) che contiene tutte le info scaricati del servizio meteo e dal ds3231
//tali dati verranno poi inviati e visualizzati dal nextion
Info gDati = Info();

//usata per richiamare e aggiornare le info del meteo quando si cambia città da hmi nextion
volatile bool aggiorna=false;

//indica se attualemte è giorno o notte
fase_del_di_enum fase_del_di = giorno;

void setup() 
{
  
  //CONNESSIONE AL WIFI
  WiFi.begin(ssid, password);
  int counter = 0;
  while (WiFi.status() != WL_CONNECTED) 
  {
    delay(200);    
    if (++counter > 100)
    {
      //SE NON SI CONNETTE DOPO 100 TENTATIVI, RESETTA ESP 
      ESP.restart();
    }
     
  }
  //Serial.println("\nWiFi connected");
  //printWifiStatus();

  //INIZIALIZZA COMUNICAZIONE CON DS3231 RTC
  if (!rtc.begin()) 
  {
    //errore comunicazione i2c con sensore ds3231
    rtcOK=false;   
  }
  else
  {
    //ok. sensore presente e comunicante
    rtcOK=true;
  }
  if (rtc.lostPower() && rtcOK) 
  {
    //Se dispositivo è nuovo o vi è stata caduta di potenza (batteria tampone scarica o mancante)
    //si setta la data e ora attuali come data e ora in cui questo sketch è stato compilato
    //sarà possibile cambiare data e ora tramite hmi Nextion
    rtc.adjust(DateTime(F(__DATE__), F(__TIME__)));
  }

  //NEXTION HMI inizializzazione seriale
  myNex.begin(9600);

  //AVVIO TIMER PER HTTP GET
  httpGetTimer->start();
}

void loop() {

  //LEGGO DATA E ORA ATTUALE dal ds3231
  if(rtcOK)
  {
    DateTime dt_now = rtc.now();
    char dt[21] = "DD/MM/YYYY hh:mm:ss";
    char solo_data[12] = "DD/MM/YYYY";
    char solo_ora[21] = "hh:mm:ss";
    gDati.dt = dt;
    gDati.solo_data = solo_data;
    gDati.solo_ora = solo_ora;
    dt_now.toString(gDati.dt);
    dt_now.toString(gDati.solo_data);
    dt_now.toString(gDati.solo_ora);

    //determinazione del giorno o notte
    //dopo le 19 è notte
    DateTime soglia_notte(dt_now.year(),dt_now.month(),dt_now.day(),19,0,0);
    //dopo le 6am è giorno
    DateTime soglia_giorno(dt_now.year(),dt_now.month(),dt_now.day(),6,0,0);

    if(dt_now>=soglia_giorno && dt_now<=soglia_notte)
    {
      fase_del_di=giorno;      
    }
    else
    {
      fase_del_di=notte;
    }

  }
 
  if (httpGetTimer->getET()>httpGetInterval || first_scan || aggiorna)
  {
    first_scan=false;
    aggiorna=false;
    httpGetTimer->reset();
    //OTTENGO LAT E LON DATA LA CITTA' E IL COUNTRY CODE (in questo caso il country-code è fisso e pari a IT = Italia)
    bool ok=GetCityLatLon(CITY_NAME, gDati);
    if(!ok)
      return;
    delay(1500);
    //OTTENGO LE INFORMAZIONI SUL METEO DATI LAN E LON
    ok = updateInfoTempo(gDati);
    

    //AGGIORNA TEMPERATURA INTERNA TRAMITE DS3231
    if(rtcOK)
    {
      gDati.temperatura_interna = rtc.getTemperature();
    }
    
  }
  
  //INVIO TUTTI I DATI AGGIRONATI AL NEXTION
  updateDatiNextion(gDati);
  //FUNZIONE NECESSARIA DA CHIAMARE NEL LOOP PER GESTIONE RICEZIONE DATI DAL NEXTION
  myNex.NextionListen();
  delay(100);

}

bool updateDatiNextion(Info& Dati)
{
 
 //SCRITTURA DEI CAMPI TESTUALI PRESENTI NALLA PAGINA PRINCIPALE DEL NEXTION (PAGE 0)
  //myNex.writeStr("dataora.txt", String(Dati.dt));
  myNex.writeStr("solodata.txt", String(Dati.solo_data));
  myNex.writeStr("soloora.txt", String(Dati.solo_ora));
  myNex.writeStr("NomeCitta.txt", String(Dati.nomecitta_it));
  myNex.writeStr("DescTempo.txt", String(Dati.descrizione_meteo));
  myNex.writeStr("TempEsterna.txt", String(Dati.temperatura_esterna,1));
  myNex.writeStr("TempInterna.txt", String(Dati.temperatura_interna,1));
  myNex.writeStr("UmiditaEsterna.txt", String(Dati.umidita_esterna,0));
  myNex.writeStr("vel_vento.txt", String(Dati.velocita_vento*3.6,1));//3.6 conversione m/s -> km/h
  
  //CONVERSIONE DEL ANGOLO DEL VENTO [0-360°) IN DIREZIONE TESTUALE
  //IN BASE ALLA DIREZIONE SI SCEGLI L'IMMAGINE DA VISUALIZZARE SUL NEXTION
  String compass = degToCompass8(Dati.direzione_vento);
  if(compass == "N")
  {
    myNex.writeNum("IconaVento.pic", 9);
  }
  else if(compass == "NE")
  {
    myNex.writeNum("IconaVento.pic", 10);
  }
  else if(compass == "E")
  {
    myNex.writeNum("IconaVento.pic", 11);
  }
  else if(compass == "SE")
  {
    myNex.writeNum("IconaVento.pic", 12);
  }
  else if(compass == "S")
  {
    myNex.writeNum("IconaVento.pic", 13);
  }
  else if(compass == "SW")
  {
    myNex.writeNum("IconaVento.pic", 14);
  }
  else if(compass == "W")
  {
    myNex.writeNum("IconaVento.pic", 15);
  }
  else if(compass == "NW")
  {
    myNex.writeNum("IconaVento.pic", 16);
  }

  //IN BASE ALL'ID METEO RICEVUTA DAL SERVIZIO https://openweathermap.org/
  //SI SCEGLIE QUALI ICONA VISUALIZZARE SUL NEXTION
  //LA CONVERSIONE id-IMMAGINE è STATA FATTA USANDO LE TABELLE AL SEGUENTE LINK
  //https://openweathermap.org/weather-conditions#How-to-get-icon-URL
  int id_icona_meteo=0;
  if(Dati.id_icona_meteo == 800)    //SERENO NESSUNA NUVOLA
  {
    
    if(fase_del_di==giorno)
    {
      id_icona_meteo=0;
    }
    else
    {
      id_icona_meteo=2;
    }
    myNex.writeNum("IconaTempo.pic", id_icona_meteo);
  }
  else if (Dati.id_icona_meteo == 801)
  {
    //SOLE+NUVOLE
    if(fase_del_di==giorno)
    {
      id_icona_meteo=1;
    }
    else
    {
      id_icona_meteo=3;
    }
    myNex.writeNum("IconaTempo.pic", id_icona_meteo);
  }
  else
  {
    switch(Dati.id_icona_meteo/100)//divisione fra interi
    {
      case 2:     //TEMPESTA
          id_icona_meteo=6;
          myNex.writeNum("IconaTempo.pic", id_icona_meteo);
          break;
  
      case 3:     //PIOGGERELLINA
          id_icona_meteo=4;
          myNex.writeNum("IconaTempo.pic", id_icona_meteo);
          break;
      case 5:     //PIOGGIA
          id_icona_meteo=5;
          myNex.writeNum("IconaTempo.pic", id_icona_meteo);
          break;

      case 6:    //NEVE
          id_icona_meteo=7;
          myNex.writeNum("IconaTempo.pic", id_icona_meteo);
          break;
  
      case 7:     //NEBBIA
          id_icona_meteo=8;               
          myNex.writeNum("IconaTempo.pic", id_icona_meteo);
          break;
      case 8:     //NUVOLOSO
          id_icona_meteo=21;
          myNex.writeNum("IconaTempo.pic", id_icona_meteo);
          break;
    }    
  }
  return true;
}

bool GetCityLatLon(String NomeCitta, Info& Dati)
{

  //DATO IL NOME CITTA E IL COUNTRY CODE SI OTTENGOLO LAT E LON
  bool ok=false;

  String GET_URI_LATLON_BY_NAME = "GET /geo/1.0/direct?q="+NomeCitta+","+COUNTRY_CODE+"&limit=1&appid="+API_KEY+" HTTP/1.1";//solo la prima città limit=1;
  
  if (client.connect("api.openweathermap.org", 80)) 
  {
    
    // INVIO RICHIESTA HTTP GET AL SERVER:
    client.println(GET_URI_LATLON_BY_NAME);
    client.println("Host: api.openweathermap.org");
    client.println("Connection: close");
    client.println();

    // CONTROLLO DELLO STATO HTTP
    char status[32] = {0};
    client.readBytesUntil('\r', status, sizeof(status));
    // DOVREBBE ESSERE "HTTP/1.0 200 OK" oppure "HTTP/1.1 200 OK"
    if (strcmp(status + 9, "200 OK") != 0) 
    {
      return ok;
    }

    // SALTA GLI HTTP headers
    char endOfHeaders[] = "\r\n\r\n";
    if (!client.find(endOfHeaders)) 
    {
      //Serial.println(F("Invalid response"));
      return ok;
    }
    //IN RISPOSTA ABBIaMO UNA STRINGA JSON
    //CONVERSIONE DELLA STRINGA NEL OGGETO JSON
    //TRAMITE LA LIBRERIA https://arduinojson.org/v6/assistant/#/step1
    //LE DIMENSIONE DEL JSON (ESEMPIO 3072) sono state calcolate usando il link sopra
    //copiando la risposta json dal sito https://openweathermap.org/current#geo
    DynamicJsonDocument doc(3072);
    DeserializationError error = deserializeJson(doc, client);
    if (error) {
      //Serial.print(F("deserializeJson() failed: "));
      //Serial.println(error.f_str());
      return ok;
    }

    //UNPACK
    JsonObject root_0 = doc[0];
    const char* root_0_name = root_0["name"]; // "London"
    JsonObject root_0_local_names = root_0["local_names"];
    const char* root_0_local_names_it = root_0_local_names["it"]; // "Londra"

    float root_0_lat = root_0["lat"]; // 51.5085
    float root_0_lon = root_0["lon"]; // -0.1257
    const char* root_0_country = root_0["country"]; // "GB"

    Dati.lat = root_0_lat;
    Dati.lon = root_0_lon;
    Dati.country_id = root_0_country;
    //Dati.nomecitta_it = root_0_local_names_it;
    Dati.nomecitta_en = root_0_name;
    //Serial.println(String(Dati.nomecitta_it)+" lat: "+String(Dati.lat,2)+" lon: "+String(Dati.lon,2)+" "+String(Dati.country_id));

    ok=true;
    return ok;
  } 
  else 
  {
    //CONNESSIONE FALLITA
    return ok;
  }
}

bool updateInfoTempo(Info& Dati)
{

  //DATI LAT E LON SI OTTENGONO LE INFORMAZIONI SUL METEO
  bool ok=false;
  String GET_URI_WEATHER_BY_LATLON = "GET /data/2.5/weather?lat="+String(Dati.lat,2)+"&lon="+String(Dati.lon,2)+"&units=metric&lang=it&appid="+API_KEY+" HTTP/1.1";
  //https://api.openweathermap.org/data/2.5/weather?lat=44.34&lon=10.99&appid={API key}

  
  //se connessione è ok
  if (client.connect("api.openweathermap.org", 80)) 
  {
    // INVIO RICHIESTA HTTP GET AL SERVER:
    client.println(GET_URI_WEATHER_BY_LATLON);
    client.println("Host: api.openweathermap.org");
    client.println("Connection: close");
    client.println();

    // CONTROLLO DELLO STATO HTTP
    char status[32] = {0};
    client.readBytesUntil('\r', status, sizeof(status));
    // It should be "HTTP/1.0 200 OK" or "HTTP/1.1 200 OK"
    if (strcmp(status + 9, "200 OK") != 0) 
    {
      return ok;
    }

    // SALTA GLI HTTP headers
    char endOfHeaders[] = "\r\n\r\n";
    if (!client.find(endOfHeaders)) 
    {
      //Serial.println(F("Invalid response"));
      return ok;
    }

    //IN RISPOSTA ABBIAMO UNA STRINGA JSON
    //CONVERSIONE DELLA STRINGA NEL OGGETO JSON
    //TRAMITE LA LIBRERIA https://arduinojson.org/v6/assistant/#/step1
    //LE DIMENSIONE DEL JSON (ESEMPIO 1024) sono state calcolate usando il link sopra
    //copiando la risposta json dal sito https://openweathermap.org/current#geo
    StaticJsonDocument<1024> doc;

    DeserializationError error = deserializeJson(doc, client);
    
    if (error) {
     
      return ok;
    }
    
    JsonObject weather_0 = doc["weather"][0];
    Dati.id_icona_meteo = weather_0["id"]; // 501
    const char* principale_meteo=weather_0["main"];
    const char* descrizione_meteo= weather_0["description"];
    Dati.principale_meteo = principale_meteo; // "Rain"
    Dati.descrizione_meteo = descrizione_meteo; // "moderate rain"
    //const char* weather_0_icon = weather_0["icon"]; // "10d"
    //const char* base = doc["base"]; // "stations"    
    JsonObject main = doc["main"];
    Dati.temperatura_esterna = main["temp"]; // 298.48   
    Dati.temperatura_percepita= main["feels_like"]; // 298.74
    Dati.temperatura_minima = main["temp_min"]; // 297.56
    Dati.temperatura_massima = main["temp_max"]; // 300.05
    Dati.pressione = main["pressure"]; // 1015
    Dati.umidita_esterna= main["humidity"]; // 64
    //int main_sea_level = main["sea_level"]; // 1015
    //int main_grnd_level = main["grnd_level"]; // 933   
    Dati.visibilita = doc["visibility"]; // 10000
    JsonObject wind = doc["wind"];
    Dati.velocita_vento = wind["speed"]; // 0.62
    Dati.direzione_vento = wind["deg"]; // 349
    Dati.nomecitta_it = doc["name"]; // "Zocca"

    //Disconnect
    client.stop();

    ok=true;
    return ok;
  } 
  else 
  {
    //CONNESSIONE FALLITA
    return ok;
  }
}

void printWifiStatus() 
{
  // print the SSID of the network you're attached to:
  //Serial.print("SSID: ");
  //Serial.println(WiFi.SSID());

  // print your board's IP address:
  IPAddress ip = WiFi.localIP();
  //Serial.print("IP Address: ");
  //Serial.println(ip);

  // print the received signal strength:
  long rssi = WiFi.RSSI();
  //Serial.print("signal strength (RSSI):");
  //Serial.print(rssi);
  //Serial.println(" dBm");
}

/***************************** CALLBACKS HMI NEXTION -> ARDUINO *********************************************************/
//ESISTONO 51 CALLBACKS CONFIGURABILI DA trigger0() A trigger50() CHE POSSONO ESSERE CHIAMATE
//INVIANDO DAL NEXTION LA STRINGA "printh 23 02 54 xx" DOVE xx è ID DELLA FUNZIONE triggerXX() in HEX
//esempio trigger2() -> "printh 23 02 54 02" trigger15() -> "printh 23 02 54 0F" ...

 //CALLBACK PER CAMBIARE CITTA' (BOTTONE OBJNAME:CambiaCitta)
void trigger0(){
  // PER CHIAMRE QUESTA FUNZIONE SCRIVI NEL "RELEASE EVENT" DI UN BOTTONE DEL NEXTION LA STRINGA "printh 23 02 54 00"

  String NomeCittaInputato = myNex.readStr("InsNomeCitta.txt");
  if(NomeCittaInputato=="ERROR")
  {
    //errore
    return;
  }
    
  CITY_NAME = NomeCittaInputato;
  aggiorna=true;
  
}
//CALLBACK PER IL SETTAGGIO DATA-ORA
void trigger1(){
  // PER CHIAMRE QUESTA FUNZIONE SCRIVI NEL "RELEASE EVENT" DI UN BOTTONE DEL NEXTION LA STRINGA "printh 23 02 54 01"
 // January 21, 2014 at 3am dovrà essere chiamata:
 // rtc.adjust(DateTime(2014, 1, 21, 3, 0, 0));
  int anno=myNex.readNumber("anno.val");
  int mese=myNex.readNumber("mese.val");
  int giorno=myNex.readNumber("giorno.val");
  int ore=myNex.readNumber("ore.val");
  int minuti=myNex.readNumber("minuti.val");
  int secondi=myNex.readNumber("secondi.val");
  if (anno==777777 || mese==777777 || giorno==777777 || ore==777777|| minuti==777777|| secondi==777777)
  {
    //errore
    return;
  }
  //SETTA DATA-ORA NEL DS3231
  rtc.adjust(DateTime(anno, mese, giorno, ore, minuti, secondi));
 
}


