# Firmware Carte Séquenceur – Fusée Expérimentale CNES C'Space 2026

## Contexte

Ce firmware pilote la **carte séquenceur** d'une fusée expérimentale participant au
**C'Space 2026**, campagne de tirs organisée par le CNES du **11 au 18 juillet 2026**
sur la base d'Aire-sur-l'Adour.

Le rôle principal de cette carte est :
1. Détecter le retrait du **jack de sécurité** (lors de la séparation en vol)
2. Déclencher, après un délai configurable, l'ouverture du **servo moteur** (déploiement parachute)
3. Permettre, au sol, la **préparation du lanceur** (fermeture/ouverture manuelle de la trappe parachute via servo)

La **carte expérience** n'est pas encore réalisée ; les entrées associées (SW_EXPERIENCE) sont désactivées en v1.

---

## Matériel cible

| Composant | Référence |
|-----------|-----------|
| Microcontrôleur | **NUCLEO-L432KC** (STM32L432KC) |
| Framework | Mbed OS (PlatformIO) |
| Vitesse UART debug | 115200 bauds |

> **Note plateforme** : Le cahier des charges initial mentionnait l'Arduino Nano Every.
> Le NUCLEO-L432KC a été retenu car il possède le **même pinout** et les mêmes
> caractéristiques électriques, tout en offrant un débogueur/programmeur intégré (ST-LINK).

---

## Mapping des broches

| Signal | Broche | Direction | Note |
|--------|--------|-----------|------|
| SW_CALCULO | D2 | Entrée | Switch mode calcul |
| SW_EXPERIENCE | D3 | Entrée | **Réservé v2** – voir workaround ci-dessous |
| SW_SYSTEME | D4 | — | **Non géré firmware** – voir note ci-dessous |
| JACK_TRG | D5 | Entrée | Jack de sécurité (active low) |
| LED_EXP | D6 | Sortie | LED état expérience |
| LED_JACK | D7 | Sortie | LED état jack |
| BP_SERVO | D8 | Entrée | **Non câblé v1** – voir workaround ci-dessous |
| SERVO | D9 | Sortie PWM | Servo moteur (1000–2000 µs) |

---

## Notes importantes – À corriger pour la v2

### SW_SYSTEME (D4) – Non géré par le firmware
SW_SYSTEME est le **commutateur d'alimentation général** de la carte.
Son rôle (allumer/éteindre l'électronique) est géré au niveau **matériel**.
Le firmware ne le lit pas : quand la carte est alimentée, le programme tourne en continu.

### BP_SERVO (D8) – Non câblé sur la carte v1
Le bouton poussoir de commande manuelle du servo **n'a pas été câblé** sur la
carte v1. En attendant la v2 :

**Workaround actif** : la broche **D3 (SW_EXPERIENCE) est réutilisée comme BP_SERVO**.
- Appuyer sur le bouton SW_EXPERIENCE au sol → ouvre ou ferme le servo (toggle)
- Le mode EXPERIENCE est désactivé en v1 (carte expérience non réalisée)
- Cette affectation temporaire est définie dans `PIN_BP_SERVO = D3` dans `main.cpp`

**Actions pour la v2 :**
- Câbler un bouton dédié sur D8
- Changer `PIN_BP_SERVO = D8` dans `main.cpp`
- Restaurer `DebouncedInput swExperience` et le mode EXPERIENCE

---

## États de l'automate

| État | Description |
|------|-------------|
| `ARRET` | Réservé (alimentation coupée) |
| `ATTENTE` | Système prêt, en attente de conditions |
| `MODE_CALCULO` | Mode calcul sélectionné |
| `MODE_EXPERIENCE` | Mode expérience sélectionné (désactivé v1) |
| `TIMER_EN_COURS` | Temporisation avant ouverture auto (5 s) |
| `SERVO_OUVERT` | Trappe ouverte |
| `SERVO_FERME` | Trappe fermée manuellement |

---

## Commandes de maintenance UART

Envoyer un caractère via le terminal série (115200 bauds) :

| Commande | Action |
|----------|--------|
| `h` | Aide |
| `s` | Afficher l'état système |
| `i` | Afficher les entrées |
| `o` | Ouvrir le servo |
| `c` | Fermer le servo |
| `t` | Toggle servo |

---

## Dépendances (bibliothèques locales)

| Bibliothèque | Rôle |
|---|---|
| `DebouncedInput` | Anti-rebond logiciel sur les entrées |
| `ServoOutput` | Commande servo (angle → PWM) |
| `UartLogger` | Logger UART non bloquant |
