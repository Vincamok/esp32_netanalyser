# ESP32 Network Tester

Testeur réseau autonome basé sur un ESP32-S3 avec écran TFT, connecté au réseau via une carte Ethernet en USB-C.

## Matériel requis

| Composant | Modèle recommandé | Notes |
|-----------|-------------------|-------|
| Microcontrôleur | LilyGo T-Display-S3 | ESP32-S3 + écran ST7789 1.9" intégré |
| Carte réseau | Module W5500 SPI | Connectée aux pins SPI de l'ESP32 |
| Adaptateur USB-C | USB-C → Breakout SPI | Pour brancher la carte réseau côté USB-C du boîtier |
| Câble RJ45 | Cat5e ou supérieur | |

### Pourquoi W5500 et non USB-ETH direct ?

L'ESP32 classique ne dispose pas de contrôleur USB Host. L'ESP32-S3 peut faire du USB Host mais le driver CDC-ETH (AX88179, CH9434) n'est pas encore stable dans Arduino. La solution **W5500 via SPI** est robuste, rapide (100 Mbps full-duplex) et bien supportée.

L'adaptateur USB-C est utilisé uniquement comme **connecteur mécanique** (boîtier) : les signaux SPI passent en interne par un câble depuis l'USB-C vers les pins ESP32.

## Câblage W5500 → ESP32-S3

```
W5500    ESP32-S3
------   --------
MOSI  →  GPIO 11
MISO  →  GPIO 13
SCK   →  GPIO 12
CS    →  GPIO 10
INT   →  GPIO  9
RST   →  GPIO  8
VCC   →  3.3V
GND   →  GND
```

## Installation

```bash
# Prérequis : PlatformIO CLI
pip install platformio

cd esp32-network-tester

# Compiler et flasher
pio run -e lilygo-t-display-s3 -t upload

# Moniteur série
pio device monitor
```

## Pages d'affichage

Navigation : bouton **UP** = page précédente, **DOWN** = page suivante. **Long-press UP** (> 1s) = relancer tous les tests.

| Page | Contenu |
|------|---------|
| **Résumé** | Vue d'ensemble de tous les tests |
| **Détails IP** | IP, masque, gateway, DNS, MAC, duplex/vitesse |
| **Ping** | RTT vers gateway, 8.8.8.8, 1.1.1.1 + résolution DNS |
| **Débit** | Download/Upload Mbps avec barres de progression |
| **Traceroute** | ICMP traceroute vers 8.8.8.8 (max 15 sauts) |
| **Scanner** | Hôtes ARP détectés sur le /24 avec fabricant OUI |
| **Sécurité** | IP publique, détection DNS leak, ARP spoof, proxy |
| **Câble** | Diagnostic paires A/B/C/D + longueur TDR (DP83848) |
| **Historique** | 5 derniers tests depuis LittleFS |
| **QR Code** | QR encodant IP/GW/débit/ping pour partager |
| **Webserver** | URL d'accès au serveur web embarqué |

## Serveur web embarqué (port 80)

| Route | Description |
|-------|-------------|
| `GET /` | Page HTML complète, auto-refresh 5s |
| `GET /api/status` | JSON complet de l'état réseau |
| `GET /api/scan` | JSON des hôtes ARP scannés |
| `GET /api/traceroute` | JSON du traceroute |
| `GET /wol?mac=XX:XX:XX:XX:XX:XX` | Envoyer un magic packet Wake-on-LAN |

## Fonctionnalités avancées

- **Scan ARP** : connexion TCP sur chaque .1-.254, OUI lookup sur 30 fabricants majeurs
- **Port scan** : teste les ports 22/23/25/53/80/443/445/3389/8080/8443 sur la gateway
- **DHCP rogue** : envoie un DHCPDISCOVER et collecte toutes les offres (> 1 = alerte)
- **Wake-on-LAN** : magic packet UDP port 9 broadcast
- **mDNS** : écoute 3s les annonces `_http._tcp`, `_printer._tcp`, `_smb._tcp`
- **Jitter** : 20 pings vers 8.8.8.8, écart-type des RTT
- **MTU optimal** : binary search ICMP DF bit de 576 à 1500
- **Captive portal** : GET connectivitycheck.gstatic.com/generate_204
- **DNS leak** : comparaison IP publique ipify vs Cloudflare
- **ARP spoof** : comparaison MAC gateway entre deux scans
- **Historique** : 10 entrées JSON dans LittleFS
- **QR Code** : lib ricmoo/qrcode, affiché centré sur TFT

## Codes couleur

- Vert : OK
- Jaune : Avertissement (APIPA, perte partielle, débit faible)
- Rouge : Échec
- Bleu : Test en cours

## Tests effectués

1. **Lien Ethernet** — Câble, vitesse, duplex
2. **Configuration IP** — DHCP, APIPA
3. **Ping gateway** — RTT, taux de perte
4. **DNS** — Résolution + ping 8.8.8.8/1.1.1.1
5. **Internet** — HTTP detectportal.firefox.com
6. **Débit** — HTTP download
7. **Traceroute** — ICMP TTL 1..15
8. **Scanner ARP** — /24, OUI lookup
9. **Sécurité** — IP publique, DNS leak, ARP spoof, proxy
10. **Câble** — Paires TDR (DP83848 seulement)

## Câblage DP83848 RMII (diagnostic câble TDR)

Pour activer le diagnostic câble TDR, utiliser le profil `lilygo-t-display-s3-dp83848` qui définit `USE_DP83848_RMII`.

```
DP83848  ESP32
-------  -----
MDC   →  GPIO 23
MDIO  →  GPIO 18
CLK   →  GPIO 0  (RMII REF_CLK)
RXD0  →  GPIO 25
RXD1  →  GPIO 26
CRS   →  GPIO 27
TXD0  →  GPIO 19
TXD1  →  GPIO 22
TX_EN →  GPIO 21
```

Le W5500 ne supporte pas le TDR. Les fonctions `cable_diag` retournent `N/A` si `USE_W5500_SPI` est défini.

## Rafraîchissement

Ping, DNS et internet sont relancés automatiquement toutes les **30 secondes**. Les tests lourds (scan, traceroute, sécurité) s'exécutent une fois au démarrage et sur **long-press UP**.

## Adaptation à d'autres cartes

Modifier `include/config.h` et `platformio.ini`. Boards supportées :

- `lilygo-t-display-s3` (W5500 SPI, défaut)
- `lilygo-t-display-s3-dp83848` (DP83848 RMII, diagnostic câble TDR)
- `ttgo-t-display` (ESP32 classique, écran 1.14")
