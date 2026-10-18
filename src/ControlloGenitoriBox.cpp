#include <Arduino.h>

#include "settings.h"

#include "ControlloGenitoriBox.h"

#include "AudioPlayer.h"
#include "Log.h"
#include "SdCard.h"
#include "System.h"

#include <esp_random.h>
#include <time.h>

// ============================================================
//  Innesto del controllo genitori (task 6).
//
//  Qui dentro ci sono solo i fili verso il box: l'ora, la NVS,
//  il volume, il messaggio della nanna. Le decisioni le prende
//  ControlloGenitori, che non sa niente di Arduino ed e' provato
//  sul Mac con ./firmware/prove.sh.
// ============================================================

namespace {

constexpr const char *CARTELLA_NANNA = "/nanna";
constexpr uint16_t VERSIONE_FORMATO = 1;

// Le struct vanno in NVS come blob: il numero di versione evita di
// rileggere spazzatura se un domani cambiamo i campi.
struct BloccoRegole {
	uint16_t versione;
	box::Regole regole;
};

struct BloccoStato {
	uint16_t versione;
	box::Stato stato;
};

box::Regole gRegole;
box::ControlloGenitori *gControllo = nullptr;
uint8_t gVolumeMassimoGenitori = 0; // quello impostato dai genitori, senza fasce
int gVolumeApplicato = -1;          // ultimo limite scritto nel player

// ------------------------------------------------------------------
// Ora del box
// ------------------------------------------------------------------
// ESPuino tiene l'ora nell'RTC interno dell'ESP32 e la sincronizza via NTP.
// Senza sincronizzazione l'anno resta il 1970: in quel caso l'ora non e'
// affidabile e il modulo applica la regola piu' restrittiva.
// Quando ci sara' il DS3231 (hardware/pinout.md: SDA 43, SCL 44) bastera'
// riempire qui la struct leggendo da lui.
box::Orario adesso() {
	box::Orario o;
	struct tm t;
	if (!getLocalTime(&t, 50)) {
		o.affidabile = false;
		return o;
	}
	const int anno = t.tm_year + 1900;
	o.minuti = t.tm_hour * 60 + t.tm_min;
	o.giornoSettimana = (t.tm_wday + 6) % 7; // tm_wday: 0 = domenica
	o.affidabile = (anno >= 2024);

	// Giorni dal 1970: serve solo a distinguere una fascia dall'altra.
	struct tm mezzanotte = t;
	mezzanotte.tm_hour = 0;
	mezzanotte.tm_min = 0;
	mezzanotte.tm_sec = 0;
	const time_t inizioGiorno = mktime(&mezzanotte);
	o.giornoEpoca = static_cast<int>(inizioGiorno / 86400);
	return o;
}

// ------------------------------------------------------------------
// Memoria non volatile
// ------------------------------------------------------------------
void salvaStato() {
	if (gControllo == nullptr) {
		return;
	}
	BloccoStato blocco {VERSIONE_FORMATO, gControllo->stato()};
	gPrefsSettings.putBytes("genStato", &blocco, sizeof(blocco));
}

box::Stato leggiStato() {
	BloccoStato blocco {};
	const size_t letti = gPrefsSettings.getBytes("genStato", &blocco, sizeof(blocco));
	if (letti != sizeof(blocco) || blocco.versione != VERSIONE_FORMATO) {
		return box::Stato {};
	}
	return blocco.stato;
}

box::Regole leggiRegole() {
	BloccoRegole blocco {};
	const size_t letti = gPrefsSettings.getBytes("genRegole", &blocco, sizeof(blocco));
	if (letti != sizeof(blocco) || blocco.versione != VERSIONE_FORMATO) {
		// Primo avvio: nessuna fascia, box libero. Le fasce si impostano
		// dalla pagina "Controllo genitori" della web UI.
		box::Regole predefinite;
		predefinite.volumeMassimoNormale = AudioPlayer_GetMaxVolumeSpeaker();
		return predefinite;
	}
	return blocco.regole;
}

// ------------------------------------------------------------------
// Volume
// ------------------------------------------------------------------
// Il cursore arriva al massimo impostato dai genitori; dentro una fascia
// arriva solo al limite ridotto. Non tocchiamo il valore salvato dei
// genitori: cambiamo solo il tetto che il player applica adesso.
void applicaVolume(int massimo) {
	if (massimo <= 0 || massimo == gVolumeApplicato) {
		return;
	}
	gVolumeApplicato = massimo;
	AudioPlayer_SetMaxVolume(static_cast<uint8_t>(massimo));
	if (AudioPlayer_GetCurrentVolume() > massimo) {
		AudioPlayer_SetCurrentVolume(static_cast<uint8_t>(massimo));
	}
	Log_Printf(LOGLEVEL_INFO, "Controllo genitori: volume massimo a %d", massimo);
}

// ------------------------------------------------------------------
// Messaggio della nanna: un file a caso da /nanna
// ------------------------------------------------------------------
bool riproduciMessaggioNanna() {
	File cartella = gFSystem.open(CARTELLA_NANNA);
	if (!cartella || !cartella.isDirectory()) {
		Log_Println("Controllo genitori: manca la cartella /nanna, blocco in silenzio", LOGLEVEL_ERROR);
		return false;
	}

	// Scelta a caso in un passaggio solo (campionamento del flusso): non
	// serve tenere in memoria l'elenco, che su una SD puo' essere lungo.
	char scelto[256] = "";
	uint32_t visti = 0;
	File file = cartella.openNextFile();
	while (file) {
		if (!file.isDirectory() && file.size() > 0) {
			visti++;
			if (esp_random() % visti == 0) {
				snprintf(scelto, sizeof(scelto), "%s", file.path());
			}
		}
		file = cartella.openNextFile();
	}

	if (scelto[0] == '\0') {
		Log_Println("Controllo genitori: nessun messaggio in /nanna", LOGLEVEL_ERROR);
		return false;
	}

	Log_Printf(LOGLEVEL_NOTICE, "Controllo genitori: riposo, riproduco %s", scelto);
	return AudioPlayer_PlayAnnouncement(scelto);
}

void applicaDecisione(const box::Decisione &d) {
	applicaVolume(d.volumeMassimo);
	salvaStato();
}

} // namespace

// ------------------------------------------------------------------
// Interfaccia verso il resto del firmware
// ------------------------------------------------------------------
void ControlloGenitoriBox_Init(void) {
	gVolumeMassimoGenitori = AudioPlayer_GetMaxVolumeSpeaker();
	gRegole = leggiRegole();
	static box::ControlloGenitori controllo(gRegole, leggiStato());
	gControllo = &controllo;
	Log_Println("Controllo genitori pronto", LOGLEVEL_NOTICE);
}

bool ControlloGenitoriBox_PuoPartire(const char *uid) {
	if (gControllo == nullptr) {
		return true;
	}
	const box::Decisione d = gControllo->tagAppoggiato(adesso(), uid, box::Tipo::Storia);
	applicaDecisione(d);

	if (d.esito != box::Esito::SoloMessaggio) {
		return true;
	}

	// Riposo: messaggio a volume basso, poi il box si addormenta.
	applicaVolume(d.volumeMassimo);
	riproduciMessaggioNanna();
	System_RequestSleep();
	return false;
}

bool ControlloGenitoriBox_PuoComandare(void) {
	if (gControllo == nullptr) {
		return true;
	}
	const box::Decisione d = gControllo->tastoPremuto(adesso());
	applicaDecisione(d);
	if (d.esito != box::Esito::SoloMessaggio) {
		return true;
	}
	riproduciMessaggioNanna();
	System_RequestSleep();
	return false;
}

void ControlloGenitoriBox_TagTolto(const char *uid) {
	if (gControllo == nullptr) {
		return;
	}
	gControllo->tagTolto(adesso(), uid);
	salvaStato();
}

void ControlloGenitoriBox_StoriaFinita(void) {
	if (gControllo == nullptr) {
		return;
	}
	gControllo->storiaFinita(adesso());
	salvaStato();
}

bool ControlloGenitoriBox_RiposoBloccato(void) {
	return gControllo != nullptr && gControllo->riposoBloccato(adesso());
}

bool ControlloGenitoriBox_OraNonAffidabile(void) {
	return !adesso().affidabile;
}

void ControlloGenitoriBox_NannaAdesso(int minuti) {
	if (gControllo == nullptr) {
		return;
	}
	gControllo->nannaAdesso(adesso(), minuti);
	salvaStato();
	Log_Printf(LOGLEVEL_NOTICE, "Controllo genitori: nanna per %d minuti (0 = fino al mattino)", minuti);
}

void ControlloGenitoriBox_AnnullaNanna(void) {
	if (gControllo == nullptr) {
		return;
	}
	gControllo->annullaNanna();
	salvaStato();
}

void ControlloGenitoriBox_SalvaRegole(const box::Regole &regole) {
	gRegole = regole;
	BloccoRegole blocco {VERSIONE_FORMATO, regole};
	gPrefsSettings.putBytes("genRegole", &blocco, sizeof(blocco));
	if (gControllo != nullptr) {
		gControllo->aggiornaRegole(gRegole);
	}
	// Il tetto del volume torna quello dei genitori finche' una fascia non
	// dice altro: la prossima decisione lo riallinea.
	gVolumeApplicato = -1;
	applicaVolume(gVolumeMassimoGenitori);
}

const box::Regole &ControlloGenitoriBox_Regole(void) {
	return gRegole;
}
