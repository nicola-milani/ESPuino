#include "ControlloGenitori.h"

#include <cstdio>
#include <cstring>

namespace box {
namespace {

// Copia una stringa troncandola e terminandola sempre.
void copia(char *destinazione, int capienza, const char *sorgente) {
	if (capienza <= 0) {
		return;
	}
	if (sorgente == nullptr) {
		destinazione[0] = '\0';
		return;
	}
	int i = 0;
	for (; i < capienza - 1 && sorgente[i] != '\0'; i++) {
		destinazione[i] = sorgente[i];
	}
	destinazione[i] = '\0';
}

// Minuti dal 1970: serve a confrontare istanti che scavalcano la mezzanotte.
long istante(const Orario &ora) {
	return static_cast<long>(ora.giornoEpoca) * 1440L + ora.minuti;
}

bool scavalcaMezzanotte(const Fascia &f) {
	return f.fine <= f.inizio;
}

} // namespace

ControlloGenitori::ControlloGenitori(const Regole &regole, Stato stato)
	: regole_(regole), stato_(stato) {}

void ControlloGenitori::aggiornaRegole(const Regole &regole) {
	regole_ = regole;
}

// ------------------------------------------------------------------
// Quale fascia è attiva adesso
// ------------------------------------------------------------------
ControlloGenitori::FasciaAttiva ControlloGenitori::fasciaAttiva(const Orario &ora) const {
	FasciaAttiva attiva;

	// 1. La nanna manuale vince su tutto, finché non scade.
	if (stato_.nannaFine > 0 && istante(ora) < stato_.nannaFine) {
		attiva.presente = true;
		snprintf(attiva.chiave, LUNG_CHIAVE, "n%ld", stato_.nannaFine);
		attiva.storieConcesse = regole_.storieConcesseNanna;
		attiva.volumeMassimo = regole_.volumeMassimoNanna;
		return attiva;
	}

	// 2. Senza un'ora attendibile non sappiamo in che fascia siamo:
	//    si applica la regola più restrittiva fra quelle impostate.
	if (!ora.affidabile) {
		return piuRestrittiva();
	}

	// 3. Fasce del giorno corrente che non scavalcano la mezzanotte.
	const int oggi = ora.giornoSettimana;
	for (int i = 0; i < MAX_FASCE; i++) {
		const Fascia &f = regole_.fasce[oggi][i];
		if (!f.attiva || scavalcaMezzanotte(f)) {
			continue;
		}
		if (ora.minuti >= f.inizio && ora.minuti < f.fine) {
			attiva.presente = true;
			snprintf(attiva.chiave, LUNG_CHIAVE, "g%d:%d", ora.giornoEpoca, i);
			attiva.storieConcesse = f.storieConcesse;
			attiva.volumeMassimo = f.volumeMassimo;
			return attiva;
		}
	}

	// 4. Sera di una fascia che scavalca la mezzanotte (dalle 20:30 in poi).
	for (int i = 0; i < MAX_FASCE; i++) {
		const Fascia &f = regole_.fasce[oggi][i];
		if (!f.attiva || !scavalcaMezzanotte(f)) {
			continue;
		}
		if (ora.minuti >= f.inizio) {
			attiva.presente = true;
			snprintf(attiva.chiave, LUNG_CHIAVE, "g%d:%d", ora.giornoEpoca, i);
			attiva.storieConcesse = f.storieConcesse;
			attiva.volumeMassimo = f.volumeMassimo;
			return attiva;
		}
	}

	// 5. Mattina della stessa fascia, che è cominciata ieri sera: stessa chiave,
	//    così le storie già ascoltate restano contate.
	const int ieri = (oggi + 6) % 7;
	for (int i = 0; i < MAX_FASCE; i++) {
		const Fascia &f = regole_.fasce[ieri][i];
		if (!f.attiva || !scavalcaMezzanotte(f)) {
			continue;
		}
		if (ora.minuti < f.fine) {
			attiva.presente = true;
			snprintf(attiva.chiave, LUNG_CHIAVE, "g%d:%d", ora.giornoEpoca - 1, i);
			attiva.storieConcesse = f.storieConcesse;
			attiva.volumeMassimo = f.volumeMassimo;
			return attiva;
		}
	}

	return attiva; // niente fascia: il box è normale
}

ControlloGenitori::FasciaAttiva ControlloGenitori::piuRestrittiva() const {
	FasciaAttiva attiva;
	bool trovata = false;
	int storie = 0;
	int volume = 0;

	for (int g = 0; g < 7; g++) {
		for (int i = 0; i < MAX_FASCE; i++) {
			const Fascia &f = regole_.fasce[g][i];
			if (!f.attiva) {
				continue;
			}
			if (!trovata) {
				trovata = true;
				storie = f.storieConcesse;
				volume = f.volumeMassimo;
				continue;
			}
			if (f.storieConcesse < storie) {
				storie = f.storieConcesse;
			}
			if (f.volumeMassimo > 0 && (volume == 0 || f.volumeMassimo < volume)) {
				volume = f.volumeMassimo;
			}
		}
	}

	// Nessuna fascia impostata: non c'è niente da far rispettare, il box
	// resta usabile anche senza ora. Lo si segnala comunque in web UI.
	if (!trovata) {
		return attiva;
	}

	attiva.presente = true;
	copia(attiva.chiave, LUNG_CHIAVE, "incerta");
	attiva.storieConcesse = storie;
	attiva.volumeMassimo = volume;
	return attiva;
}

// Se siamo passati a un'altra fascia (o ne siamo usciti) il conteggio riparte.
ControlloGenitori::FasciaAttiva ControlloGenitori::allinea(const Orario &ora) {
	FasciaAttiva f = fasciaAttiva(ora);
	const char *chiave = f.presente ? f.chiave : "";
	if (strcmp(chiave, stato_.fasciaCorrente) != 0) {
		copia(stato_.fasciaCorrente, LUNG_CHIAVE, chiave);
		stato_.storieUsate = 0;
		stato_.tagInCorso[0] = '\0';
		stato_.storiaInCorso = false;
	}
	return f;
}

int ControlloGenitori::volumeIn(const FasciaAttiva &f) const {
	if (!f.presente || f.volumeMassimo <= 0) {
		return regole_.volumeMassimoNormale;
	}
	return f.volumeMassimo;
}

// ------------------------------------------------------------------
// Eventi
// ------------------------------------------------------------------
Decisione ControlloGenitori::tagAppoggiato(const Orario &ora, const char *uid, Tipo) {
	const FasciaAttiva f = allinea(ora);

	Decisione d;
	d.inFascia = f.presente;
	d.oraNonAffidabile = !ora.affidabile;
	d.volumeMassimo = volumeIn(f);

	if (!f.presente) {
		d.esito = Esito::Normale;
		return d;
	}

	// 4. La stessa statuina rimessa mentre la storia concessa è in corso
	//    non è un tentativo nuovo: riprende da dov'era.
	if (stato_.storiaInCorso && uid != nullptr && strcmp(uid, stato_.tagInCorso) == 0) {
		d.esito = Esito::Riprendi;
		d.dissolvenzaFinale = (stato_.storieUsate >= f.storieConcesse);
		return d;
	}

	// 1. Prima statuina della fascia: si ascolta.
	if (stato_.storieUsate < f.storieConcesse) {
		stato_.storieUsate++;
		copia(stato_.tagInCorso, LUNG_UID, uid);
		stato_.storiaInCorso = true;
		d.esito = Esito::Concessa;
		d.dissolvenzaFinale = (stato_.storieUsate >= f.storieConcesse);
		return d;
	}

	// 3. Tutto il resto: messaggio della nanna, poi sleep.
	d.esito = Esito::SoloMessaggio;
	return d;
}

Decisione ControlloGenitori::tastoPremuto(const Orario &ora) {
	const FasciaAttiva f = allinea(ora);

	Decisione d;
	d.inFascia = f.presente;
	d.oraNonAffidabile = !ora.affidabile;
	d.volumeMassimo = volumeIn(f);

	// Durante la storia concessa i tasti funzionano: servono a cambiare
	// traccia dentro la storia che il bambino sta già ascoltando.
	if (!f.presente || stato_.storiaInCorso) {
		d.esito = Esito::Normale;
		return d;
	}

	d.esito = Esito::SoloMessaggio;
	return d;
}

void ControlloGenitori::tagTolto(const Orario &ora, const char *uid) {
	allinea(ora);
	// La storia resta "in corso" anche a statuina tolta: è in pausa, e
	// rimettendola riprende senza consumare un'altra storia.
	(void) uid;
}

void ControlloGenitori::storiaFinita(const Orario &ora) {
	allinea(ora);
	stato_.storiaInCorso = false;
	stato_.tagInCorso[0] = '\0';
}

void ControlloGenitori::nannaAdesso(const Orario &ora, int minuti) {
	if (minuti > 0) {
		stato_.nannaFine = istante(ora) + minuti;
	} else {
		// Fino al mattino: la prossima volta che scocca l'ora impostata.
		long fine = static_cast<long>(ora.giornoEpoca) * 1440L + regole_.oraDelMattino;
		if (ora.minuti >= regole_.oraDelMattino) {
			fine += 1440L;
		}
		stato_.nannaFine = fine;
	}
	// La nanna è una fascia nuova: il conteggio riparte da lì.
	allinea(ora);
}

void ControlloGenitori::annullaNanna() {
	stato_.nannaFine = 0;
}

// ------------------------------------------------------------------
// Schermata della luna
// ------------------------------------------------------------------
bool ControlloGenitori::riposoBloccato(const Orario &ora) const {
	const FasciaAttiva f = fasciaAttiva(ora);
	if (!f.presente) {
		return false;
	}
	const bool stessaFascia = (strcmp(f.chiave, stato_.fasciaCorrente) == 0);
	const int usate = stessaFascia ? stato_.storieUsate : 0;
	const bool inCorso = stessaFascia ? stato_.storiaInCorso : false;
	return !inCorso && usate >= f.storieConcesse;
}

} // namespace box
