
#define Sortie_1 9
#define S_Drv_EN 8  //Sortie_2
#define Sortie_3 7
#define Sortie_4 6
#define Sortie_5 5
#define S_Drv_Dr 4  //Sortie_6
#define S_Drv_Im 3  //Sortie_7
#define Sortie_8 2

void Init_S(byte s){pinMode(s,OUTPUT);digitalWrite(s,0);}
void Rst_S(byte s){digitalWrite(s,0);}
void Set_S(byte s){digitalWrite(s,1);}

void setup() {


Init_S(S_Drv_EN);  //Sortie_2
Init_S(S_Drv_Dr);  //Sortie_6
Init_S(S_Drv_Im);  //Sortie_7

Set_S(S_Drv_EN);

}

void loop() {

Set_S(S_Drv_Im);
delay(100);
Rst_S(S_Drv_Im);
delay(100);
}
