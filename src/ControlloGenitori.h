// ============================================================
//  ControlloGenitori.h — task 6
// ------------------------------------------------------------
//  Decide se il box può riprodurre: fasce orarie di riposo,
//  quante storie sono concesse, volume ridotto, nanna manuale.
//
//  Qui dentro non c'è niente di Arduino, di proposito: il
//  modulo riceve l'ora e lo stato dall'esterno e restituisce
//  una decisione. Così si prova sul Mac in un secondo
//  (./firmware/prove.sh) invece che a ogni giro di flash, e
//  l'innesto in ESPuino resta uno strato sottile che si limita
//  a leggere il DS3231, la NVS e a parlare col player.
//
//  Lo Stato è una struttura piatta apposta: il box la scrive
//  tale e quale nella memoria non volatile, così spegnere e
//  riaccendere non azzera il blocco.
// ============================================================
#pragma once

namespace box {

// Un vinile conta come una storia: il tipo serve solo a chi usa la decisione.
enum class Tipo { Storia, Musica };

// Quando siamo. Lo riempie chi legge il DS3231.
struct Orario {
	int giornoEpoca = 0;      // giorni dal 1970: serve solo a distinguere una fascia dall'altra
	int minuti = 0;           // minuti da mezzanotte, 0..1439
	int giornoSettimana = 0;  // 0 = lunedì ... 6 = domenica
	bool affidabile = true;   // false se il DS3231 non è mai stato sincronizzato
};

// Una fascia di riposo. Se fine <= inizio la fascia scavalca la mezzanotte
// (20:30-07:00) e resta una fascia sola: il conteggio delle storie non si
// azzera a mezzanotte.
struct Fascia {
	bool attiva = false;
	int inizio = 0;           // minuti da mezzanotte
	int fine = 0;
	int storieConcesse = 1;   // 0 = solo il messaggio
	int volumeMassimo = 0;    // 0 = vale il volume normale dei genitori
};

inline constexpr int MAX_FASCE = 3;   // per giorno
inline constexpr int LUNG_UID = 21;   // UID di un NTAG213 in esadecimale + terminatore
inline constexpr int LUNG_CHIAVE = 24;

struct Regole {
	Fascia fasce[7][MAX_FASCE] = {};
	int volumeMassimoNormale = 14;    // il limite dei genitori fuori dalle fasce
	int dissolvenzaSecondi = 30;      // sfuma gli ultimi secondi dell'ultima storia concessa
	int storieConcesseNanna = 0;      // "Nanna adesso": di default blocca subito
	int volumeMassimoNanna = 5;
	int oraDelMattino = 7 * 60;       // per la nanna "fino al mattino"
};

// Quello che il box salva in memoria non volatile.
struct Stato {
	char fasciaCorrente[LUNG_CHIAVE] = "";  // a quale fascia si riferisce il conteggio
	int storieUsate = 0;
	char tagInCorso[LUNG_UID] = "";         // la statuina della storia concessa
	bool storiaInCorso = false;             // true anche mentre è in pausa
	long nannaFine = 0;                     // minuti dal 1970 fino a cui dura la nanna manuale
};

enum class Esito {
	Normale,         // fuori fascia: nessun limite nostro
	Concessa,        // in fascia, è la storia concessa
	Riprendi,        // stessa statuina rimessa durante la storia concessa
	SoloMessaggio,   // bloccato: messaggio della nanna, poi sleep
};

struct Decisione {
	Esito esito = Esito::Normale;
	int volumeMassimo = 0;         // limite da applicare al cursore del volume
	bool dissolvenzaFinale = false;// questa storia va sfumata in chiusura
	bool inFascia = false;         // per la schermata della luna
	bool oraNonAffidabile = false; // da segnalare nella web UI
};

class ControlloGenitori {
public:
	explicit ControlloGenitori(const Regole &regole, Stato stato = {});

	// Eventi che arrivano dal box.
	Decisione tagAppoggiato(const Orario &ora, const char *uid, Tipo tipo);
	Decisione tastoPremuto(const Orario &ora);
	void tagTolto(const Orario &ora, const char *uid);
	void storiaFinita(const Orario &ora);

	// "Nanna adesso" dalla web UI: minuti = 30/60/90, oppure 0 = fino al mattino.
	void nannaAdesso(const Orario &ora, int minuti);
	void annullaNanna();

	// Le regole cambiano dal telefono e valgono dal tentativo successivo.
	void aggiornaRegole(const Regole &regole);

	// Vero quando va mostrata la luna con "È ora di dormire".
	bool riposoBloccato(const Orario &ora) const;

	const Stato &stato() const { return stato_; }
	const Regole &regole() const { return regole_; }

private:
	struct FasciaAttiva {
		bool presente = false;
		char chiave[LUNG_CHIAVE] = "";
		int storieConcesse = 0;
		int volumeMassimo = 0;
	};

	FasciaAttiva fasciaAttiva(const Orario &ora) const;
	FasciaAttiva piuRestrittiva() const;      // quando l'ora non è affidabile
	FasciaAttiva allinea(const Orario &ora);  // azzera i conteggi se la fascia è cambiata
	int volumeIn(const FasciaAttiva &f) const;

	Regole regole_;
	Stato stato_;
};

} // namespace box
