// ============================================================
//  ControlloGenitoriBox.h — innesto del task 6 in ESPuino
// ------------------------------------------------------------
//  Strato sottile fra ControlloGenitori (logica pura, provata
//  sul Mac) e il box vero: legge l'ora, salva lo stato in NVS,
//  limita il volume, riproduce il messaggio della nanna.
//
//  Tutto quello che si puo' decidere senza hardware sta
//  nell'altro file. Qui ci sono solo i fili.
// ============================================================
#pragma once

#include "ControlloGenitori.h"

// Accende o spegne l'alimentazione del LED di stato (va spenta prima del
// deep sleep: un WS2812 assorbe ~1 mA anche spento).
void Box_AlimentaLed(bool acceso);

// Carica regole e stato dalla memoria non volatile. Da chiamare in setup().
void ControlloGenitoriBox_Init(void);

// Una statuina o un vinile e' stato appoggiato: si puo' riprodurre?
// Se no, fa partire il messaggio della nanna e poi manda il box a dormire.
bool ControlloGenitoriBox_PuoPartire(const char *uid);

// E' stato premuto un tasto del bambino: si puo' eseguire il comando?
bool ControlloGenitoriBox_PuoComandare(void);

// La statuina e' stata tolta (la storia resta in pausa, non consumata).
void ControlloGenitoriBox_TagTolto(const char *uid);

// La scaletta e' finita: da qui in poi il tentativo successivo e' bloccato.
void ControlloGenitoriBox_StoriaFinita(void);

// Per il display (task 2): luna e "E' ora di dormire".
bool ControlloGenitoriBox_RiposoBloccato(void);

// Per la web UI: l'ora del box non e' attendibile, il blocco e' al massimo.
bool ControlloGenitoriBox_OraNonAffidabile(void);

// "Nanna adesso" dalla web UI: minuti = 30/60/90, 0 = fino al mattino.
void ControlloGenitoriBox_NannaAdesso(int minuti);
void ControlloGenitoriBox_AnnullaNanna(void);

// Regole aggiornate dalla web UI: valgono dal tentativo successivo.
void ControlloGenitoriBox_SalvaRegole(const box::Regole &regole);
const box::Regole &ControlloGenitoriBox_Regole(void);
