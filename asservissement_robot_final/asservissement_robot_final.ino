#include <Encoder.h>
#include <PID_v1.h>
#include <MCC.h>

#define COEFF_D 0.02356 //en cm/tourDeRoue
#define COEFF_R 0.00242 //en rad/tourDeRoue

/*Interruption*/
Encoder encGauche(2, 4);
Encoder encDroite(3, 5);
//Compteurs de pas des moteurs
double positionGauche = 0, positionDroite = 0;
double dernierepositionGauche = 0, dernierepositionDroite = 0;

/*Moteur*/
#define N_FILTRE 6
unsigned long dernierTemps, maintenant, deltaTemps, tempsInitial; //en ms
double vitesseGauche, vitesseDroite;//en tours/seconde
double vitesseLineaire, vitesseRotation;
int iGauche = 0, iDroite = 0; //indice de la vitesse actuelle
const double tour = 1633;//408.25 pas par tour
double pwmMax = 255;
double vitesseMax = 2;
MCC moteurGauche(A0, A1, 9), moteurDroite(A2, A3, 10);
bool lineaireRotation = true; //Mode de déplacement lineaire:true, rotation:false

/*Odométrie*/
double x = 0, y = 0, theta = 0, distanceLineaire, distanceRotation;
double consigneX = 0, consigneY = 0, consigneTheta = 0;
double consignePosLineaire, consignePosRotation;
double positionLineaire = 0, positionRotation = 0;

/*PID*/
double consigneVitLineaire = 0, consigneVitRotation = 0;
double commandeVitLineaire, commandeVitRotation; //la commande est le pwm envoyé sur le moteur
unsigned int echantillonnage = 2; //l'échantillonnage est l'intervalle de temps entre chaque calcul de la commande, exprimé en milliseconde

//Réglage des coefficient des PID
const double kpVit = 2000;
const double kiVit = 2000;
const double kdVit = 0;

const double kpPos = 0.05;
const double kiPos = 0;
const double kdPos = 0;

PID positionLineairePID(&positionLineaire, &consigneVitLineaire, &consignePosLineaire, kpPos, kiPos, kdPos, DIRECT);
PID positionRotationPID(&positionRotation, &consigneVitRotation, &consignePosRotation, kpPos, kiPos, kdPos, DIRECT);

PID vitesseLineairePID(&vitesseLineaire, &commandeVitLineaire, &consigneVitLineaire, kpVit, kiVit, kdVit, DIRECT);
PID vitesseRotationPID(&vitesseRotation, &commandeVitRotation, &consigneVitRotation, kpVit, kiVit, kdVit, DIRECT);

double moduloAngle(double angle) {
  double tmp = fmod(angle, TWO_PI);
  if (tmp > PI) {
    tmp -= TWO_PI;
  }
  if (tmp < -PI) {
    tmp += TWO_PI;
  }
  return tmp;
}

void getVitesseGauche() {
  vitesseGauche = (positionGauche - dernierepositionGauche) / echantillonnage * 1000 / tour;
  dernierepositionGauche = positionGauche;
}

void getVitesseDroite() {
  vitesseDroite = (positionDroite - dernierepositionDroite) / echantillonnage * 1000 / tour;
  dernierepositionDroite = positionDroite;
}

void setup() {
  /*Changement fréquence des pwm des pins 9, 10*/
  TCCR1B = (TCCR1B & 0xf8) | 0x01;

  /*Initialisation de la liaison série*/
  Serial.begin(115200);

  //Initialisation PID
  positionLineairePID.SetSampleTime(echantillonnage);
  positionLineairePID.SetOutputLimits(-vitesseMax, vitesseMax);
  positionLineairePID.SetMode(AUTOMATIC);

  positionRotationPID.SetSampleTime(echantillonnage);
  positionRotationPID.SetOutputLimits(-vitesseMax, vitesseMax);
  positionRotationPID.SetMode(AUTOMATIC);

  vitesseLineairePID.SetSampleTime(echantillonnage);
  vitesseLineairePID.SetOutputLimits(-pwmMax, pwmMax);
  vitesseLineairePID.SetMode(AUTOMATIC);

  vitesseRotationPID.SetSampleTime(echantillonnage);
  vitesseRotationPID.SetOutputLimits(-pwmMax, pwmMax);
  vitesseRotationPID.SetMode(AUTOMATIC);

  //consignePosLineaire = positionLineaire + (lineaireRotation ? sqrt(sq(x - consigneX) + sq(y - consigneY)) : 0) / COEFF_D / 1000 * tour;
  //consignePosRotation = positionRotation + moduloAngle(lineaireRotation ? atan2(consigneY - y, consigneX - x) - theta : consigneTheta - theta) / COEFF_R / 1000 * tour;
  consignePosLineaire = positionLineaire + (lineaireRotation ? 200 : 0) / COEFF_D / 1000 * tour;
  consignePosRotation = positionRotation + (lineaireRotation ? 0 : TWO_PI) / COEFF_R / 1000 * tour;
  dernierTemps = millis() - echantillonnage;

  tempsInitial = millis() - 1000;
}


void loop() {
  //Calcul des consignes tout les temps echantillonage
  maintenant = millis();
  deltaTemps = maintenant - dernierTemps;
  if (deltaTemps >= echantillonnage) {
    //Lecture des positions des moteurs
    positionGauche = encGauche.read();
    positionDroite = encDroite.read();
    positionLineaire = (positionGauche + positionDroite) / 2;
    positionRotation = positionDroite - positionLineaire;

    //Calculs des vitesses des moteurs
    getVitesseGauche();
    getVitesseDroite();
    vitesseLineaire = (vitesseGauche + vitesseDroite) / 2;
    vitesseRotation = (-vitesseGauche + vitesseDroite) / 2;

    //Odométrie
    distanceLineaire = vitesseLineaire * echantillonnage * COEFF_D;
    distanceRotation = vitesseRotation * echantillonnage * COEFF_R;
    x += cos(theta) * distanceLineaire;
    y += sin(theta) * distanceLineaire;
    theta += distanceRotation;


    dernierTemps = maintenant;
  }

  positionLineairePID.Compute();
  positionRotationPID.Compute();
  vitesseLineairePID.Compute();
  vitesseRotationPID.Compute();
  moteurGauche.bouger((int)(commandeVitLineaire - commandeVitRotation) * 0.97);
  moteurDroite.bouger((int)commandeVitLineaire + commandeVitRotation);

  //Affichage liaison série
  Serial.print(y);
  Serial.print( "0 50 ");
  Serial.println(x);
}
