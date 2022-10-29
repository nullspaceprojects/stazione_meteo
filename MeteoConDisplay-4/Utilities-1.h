#ifndef Utilities_h
#define Utilities_h


class Info{
  public:
    Info(){}
  public:
    char* dt;
    char* solo_data;
    char* solo_ora;
    const char* nomecitta_it;
    const char* nomecitta_en;
    float lat;
    float lon;
    const char* principale_meteo;
    const char* descrizione_meteo;
    float temperatura_interna;
    float temperatura_esterna;
    float umidita_interna;//%
    float umidita_esterna;
    int id_icona_meteo;
    const char* country_id;
    float temperatura_percepita;//°C
    float temperatura_massima;
    float temperatura_minima;
    float pressione;//hPa
    float velocita_vento; //m/s
    int direzione_vento;// °
    float visibilita; //m
};

String degToCompass16(float gradi)
{
    int val=int((gradi/22.5)+.5);
    String arr[16]={"N","NNE","NE","ENE","E","ESE", "SE", "SSE","S","SSW","SW","WSW","W","WNW","NW","NNW"};
    return arr[(val % 16)];
}

String degToCompass8(float gradi)
{
    int val=int((gradi/45.0)+.5);
    String arr[16]={"N","NE","E", "SE","S","SW","W","NW"};
    return arr[(val % 8)];
}

enum fase_del_di_enum
{
  giorno=0,
  notte
};




class TimerC {

  private:
        unsigned long _start_time;
        unsigned long _elapsed_time;
  
  public:
      TimerC()
      {
        _start_time = 0;
        _elapsed_time = 0;
      }

      void start()
      {
        if(_start_time != 0)
        {
          return;
        }
        _start_time = millis();
      }
      
      unsigned long getET()
      {
        //in millisecondi
        _elapsed_time = millis() - _start_time;
        return _elapsed_time;
      }
      double getETSec()
      {
        unsigned long et_milli =  getET();
        return et_milli/1000.0;
      }
      void reset()
      {
        _start_time = millis();
        _elapsed_time = 0;
      }
      void stop_()
      {
        if(_start_time == 0)
        {
          return;
        }
        _start_time = 0;
        _elapsed_time = 0;
        
      }
};


#endif
