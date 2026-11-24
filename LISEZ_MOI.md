# Smart Emergency Response System — démarrer avec Wokwi

Guide débutant • 6 octobre 2026 • Kit de simulation locale, étape 1

## 1. Ce que nous allons réaliser

Wokwi est le simulateur dans lequel tu peux programmer un ESP32 et modifier les entrées de son circuit sans acheter les composants. Tu ouvres le projet, tu lances le programme et tu provoques les situations à observer.

Ton cahier des charges a été lu dans son intégralité. Il retient un PC avec Mosquitto, Node-RED et MongoDB, deux zones physiques et huit zones simulées pour la charge. Il utilise des règles prioritaires R01 à R10, et non l'ancien score global. Le MQ-2 représente une anomalie atmosphérique : on ne sépare pas artificiellement fumée et gaz et on n'affiche pas de ppm sans calibration.

Ce kit réalise la première étape : une zone virtuelle avec acquisition, décisions locales, buzzer, voyants, charge de démonstration, armement, acquittement et réinitialisation. Il écrit une trame JSON dans le moniteur série toutes les deux secondes. Il ne publie pas encore sur MQTT. Le moniteur série est la petite console qui affiche ce que le programme écrit.

Le projet complet se construit en six étapes :

| Étape | Travail | Résultat à obtenir |
| --- | --- | --- |
| 1 — maintenant | Circuit Wokwi et règles locales | Réussir les essais de ce guide |
| 2 | Créer zone_02 avec son propre identifiant | Deux zones virtuelles distinctes |
| 3 | Installer Mosquitto et Node-RED sur le PC, choisir la connexion Wokwi | Réception d'une trame MQTT réelle |
| 4 | Implémenter les règles centrales et FlowFuse Dashboard | Mesures, incidents et défauts visibles sur PC et smartphone |
| 5 | Ajouter MongoDB, commandes, confirmations et reprise | Historique et commandes traçables |
| 6 | Ajouter huit générateurs de zones et la sécurité finale | Campagne de tests, comparaison, rapport et démonstration |

Deux adaptations sont explicites : le kit utilise Arduino C++ pour apprendre dans le navigateur, tandis que ton document prescrit ESP-IDF/ESP-MQTT pour la version finale ; et il démarre avec une zone virtuelle. Remplacer les deux zones physiques du livrable par deux zones virtuelles demande une validation de périmètre par ton encadrant. La simulation logique ne valide pas les interfaces électriques, les capteurs physiques ou CA01 tel qu'il est rédigé.

## 2. Ouvrir ton premier projet

1. Décompresse l'archive Smart_Emergency_Wokwi.zip sur ton PC.
2. Ouvre https://wokwi.com/projects/new/esp32 dans ton navigateur.
3. Dans Wokwi, ouvre sketch.ino. Efface son contenu et colle le contenu du fichier sketch.ino du kit.
4. Ouvre diagram.json. Remplace tout son contenu par celui du diagram.json du kit. Le circuit doit apparaître à droite. Tu n'as pas besoin de dessiner les fils un par un.
5. Dans le menu des fichiers de l'éditeur, crée un fichier nommé exactement rules.h. Colle son contenu depuis le kit. Le programme ne compile pas sans ce fichier.
6. Crée libraries.txt et colle son contenu. Autre possibilité : dans Library Manager, ajoute « DHT sensor library for ESPx ». Ne remplace pas cette bibliothèque par une autre bibliothèque DHT.
7. Clique sur le bouton vert de lancement ▶. La première compilation peut prendre un peu de temps.
8. Ouvre le moniteur série si nécessaire. Des lignes de texte puis des lignes JSON doivent s'afficher. Après environ quatre secondes de temps simulé, sans anomalie, tu dois lire NORMAL et OK ; le voyant vert doit s'allumer.
9. Enregistre le projet dans ton compte Wokwi si tu souhaites conserver ton travail sur le site. L'archive reste ta copie du code.

Les quatre fichiers à mettre dans Wokwi sont sketch.ino, rules.h, diagram.json et libraries.txt. Le guide, les tests PC et l'exemple JSON restent sur ton ordinateur. Importer le ZIP entier n'est pas une étape supposée par ce guide.

## 3. Comprendre le circuit

| Élément affiché | Ce qu'il représente | Comment le manipuler | GPIO |
| --- | --- | --- | --- |
| ESP32 | Le microcontrôleur de zone_01 | Il exécute sketch.ino | — |
| DHT22 | Température et humidité | Cliquer sur le capteur et régler les valeurs | 15 |
| Potentiomètre « MQ2 SIMULÉ » | Une valeur ADC atmosphérique injectée | Tourner le bouton ; lire mq2_raw dans la console | 34, ADC1 |
| Interrupteur FLAMME | Sortie numérique du capteur de flamme | Gauche = absente ; droite = détectée | 27 |
| PIR | Mouvement | Cliquer puis « Simulate Motion » | 26 |
| Interrupteur PORTE | Contact reed | Gauche = fermée ; droite = ouverte | 25 |
| Interrupteur SOURCE | Présence d'une source distincte de l'ESP32 | Gauche = absente ; droite = présente | 33 |
| Interrupteur ARMEMENT | Surveillance intrusion | Gauche = désarmée ; droite = armée | 32 |
| Interrupteur DÉFAUT DHT | Injection d'une mesure invalide | Droite = défaut ; gauche = valide | 35 |
| Bouton RESET | Demande de réinitialisation | Un clic après le retour validé des mesures | 14 |
| Bouton ACQUITTER | Confirmation locale de lecture | Un clic ; l'alarme continue | 13 |
| Voyants vert, jaune, rouge | Normal, avertissement, alarme | Observer leurs couleurs et les libellés JSON | 18, 19, 21 |
| Buzzer | Alarme locale | Intermittent en ALERT, continu en CRITICAL | 23 |
| Relais et LED bleue | Une charge de réponse de démonstration | Activés par une alarme incendie critique mémorisée | 22 |

Le potentiomètre donne une entrée contrôlable pour tester les seuils ADC. Il ne simule ni la chimie ni la consommation du MQ-2. Les interrupteurs injectent les états numériques de la flamme, du contact reed et de la source surveillée. Il ne faut pas présenter ces remplacements comme une caractérisation de capteurs réels.

Le montage est prévu pour le simulateur. Le câblage d'une future maquette réelle devra être validé séparément, notamment les niveaux 3,3 V / 5 V, les broches de démarrage, les résistances et les interfaces de commande.

## 4. Lire les résultats

Les quatre gravités sont NORMAL, WARNING, ALERT et CRITICAL. Les quatre types sont FIRE, ATMOSPHERIC, INTRUSION et POWER. Une zone peut avoir plusieurs types simultanément : les incidents restent séparés dans local.incidents. La gravité de zone correspond au maximum affiché.

Dans chaque incident, active décrit la décision courante ; memorized_or_active décrit l'état maintenu pour l'affichage et les sorties. Une alarme peut donc rester affichée lorsque sa condition a disparu. C'est la mémorisation demandée par ton cahier des charges.

local.rules contient les règles confirmées ; local.pending_rules contient celles observées une seule fois. Les confirmations nécessitent deux acquisitions successives espacées de deux secondes. Une flamme ou une valeur atmosphérique élevée produit immédiatement une ALERT provisoire à la première acquisition. Une combinaison incendie critique est CRITICAL à la deuxième acquisition. Une hausse de température isolée à 60 °C commence par WARNING en attente, puis ALERT confirmée.

La première mesure arrive vers deux secondes. mq2_ready reste false durant le préchauffage simulé, fixé à quatre secondes uniquement pour faciliter les exercices. Cette durée ne représente pas celle d'un capteur réel. Le programme ne déclare pas de santé OK tant que les canaux requis ne sont pas valides.

Le champ timestamp est null et time_valid est false, car cette première version n'a ni Wi-Fi ni synchronisation d'heure. uptime_ms fournit le temps depuis le démarrage. simulation est toujours true. Il n'y a pas de faux horodatage UTC ni de RSSI inventé.

## 5. Les paramètres de démonstration

| Paramètre | Déclenchement | Retour |
| --- | --- | --- |
| Température WARNING | À partir de 45 °C | Sous 42 °C |
| Température ALERT | À partir de 60 °C | Sous 57 °C, puis selon le niveau WARNING |
| ADC atmosphérique WARNING | À partir de 1500 | Sous 1300 |
| ADC atmosphérique élevé | À partir de 2500 | Sous 2300, puis selon le niveau WARNING |
| Acquisition | Toutes les 2 secondes | Deux acquisitions pour confirmer |
| Corrélation intrusion | Mouvement et porte observés dans les 5 secondes | Fenêtre glissante |
| Retour validé | Trois acquisitions sous les seuils de retour | Puis RESET pour les alarmes mémorisées |

Les seuils ADC 1500 et 2500 sont choisis pour la simulation. Les températures de déclenchement viennent de ton document ; leurs seuils de retour sont des paramètres d'essai ajoutés ici. L'hystérésis empêche des changements répétés autour d'un seul seuil. Les versions et paramètres doivent être figés avant ta campagne de comparaison.

## 6. Réaliser les premiers essais

Entre deux essais, redémarre la simulation ou remets toutes les entrées à la normale, attends trois acquisitions normales et clique RESET. Le redémarrage efface la RAM ; il ne démontre pas une sauvegarde des alarmes.

| Essai | Manipulation | Résultat attendu |
| --- | --- | --- |
| E01 — R10 | Température 25 °C, mq2_raw sous 1300, flamme absente, porte fermée, source présente, désarmé | Après le préchauffage : NORMAL, OK, vert |
| E02 — R06 température | Température 50 °C, atmosphère basse | Après deux acquisitions : WARNING / FIRE, jaune |
| E03 — R03 | Flamme à droite, température 25 °C, atmosphère basse | ALERT / FIRE dès la première acquisition ; buzzer intermittent |
| E04 — R04 | Température 25 °C, flamme absente, mq2_raw ≥ 2500 | ALERT / ATMOSPHERIC ; ne pas classer CRITICAL FIRE |
| E05 — R05 | Température 65 °C, flamme absente, atmosphère basse | WARNING en attente, puis ALERT / FIRE |
| E06 — R02 | Température 65 °C, mq2_raw ≥ 2500, flamme absente | ALERT provisoire, puis CRITICAL / FIRE ; buzzer continu et charge bleue |
| E07 — R01 | Même combinaison qu'E06, avec flamme détectée | CRITICAL / FIRE ; R01 et R02 confirmées |
| E08 — R06 atmosphère | Température 25 °C, mq2_raw entre 1500 et 2200 | Après deux acquisitions : WARNING / ATMOSPHERIC |
| E09 — R08 | Armer, ouvrir la porte, ne pas déclencher de mouvement | WARNING / INTRUSION ; R08 confirmée après deux acquisitions |
| E10 — R07 | Armer, ouvrir la porte et déclencher le PIR | Après deux acquisitions corrélées : ALERT / INTRUSION |
| E11 — désarmé | Désarmer avant l'essai, puis porte ouverte et mouvement | Pas de nouvel incident intrusion |
| E12 — R09 | Mettre SOURCE à gauche | Après deux acquisitions : WARNING / POWER ; l'ESP32 continue de fonctionner |
| E13 — défaut | Mettre DÉFAUT DHT à droite | Température et humidité null, santé DEGRADED ; aucune conclusion NORMAL |
| E14 — RESET refusé | Pendant E06, cliquer RESET | REJECTED ; buzzer et relais maintenus |
| E15 — acquittement | Pendant E06, cliquer ACQUITTER | Lecture confirmée ; buzzer et relais maintenus |
| E16 — fin d'alarme | Après E06, remettre température 25 °C, atmosphère sous 1300, flamme absente, source présente, porte fermée et sans mouvement ; attendre au moins trois acquisitions, puis RESET | EXECUTED, NORMAL, buzzer et charge arrêtés |
| E17 — simultané | Combiner E06, E10 et E12 | FIRE, INTRUSION et POWER visibles séparément ; gravité maximale CRITICAL |

Premier exercice conseillé : E01, puis E06, puis E14, puis E16. Il montre la chaîne complète entrée → décision → sortie → maintien → réinitialisation.

Pour E06, clique sur le potentiomètre puis utilise End pour le placer au maximum si besoin. Vérifie la valeur mq2_raw affichée ; ne confonds pas l'échelle graphique du potentiomètre avec l'ADC 12 bits de l'ESP32. La flamme peut rester absente : R02 est justement prévue pour ce cas.

Le PIR reste actif cinq secondes après « Simulate Motion ». Sa fenêtre de corrélation peut prolonger l'attente de retour à la normale. Après une intrusion, fermer la porte ou désarmer ne supprime pas une ALERT déjà mémorisée : un RESET est toujours nécessaire.

Une ALERT atmosphérique provisoire observée avant la confirmation incendie peut rester mémorisée aux côtés de FIRE. Consulte active et memorized_or_active pour distinguer les conditions courantes des alarmes conservées.

## 7. Enregistrer des preuves pour ton rapport

Pour chaque essai, note la version sim-local-1.0, les paramètres, les entrées, le résultat attendu, le résultat observé et le temps simulé. Capture le circuit et une ligne JSON. N'écris pas « test réussi » avant de l'avoir effectivement exécuté.

Le fichier fiche_tests.csv fournit un tableau à compléter. Les essais PC inclus vérifient 44 assertions : règles, seuils, confirmation, hystérésis, mesures invalides, données périmées, doublons et incidents simultanés. Ils ont été exécutés avec succès pendant la préparation. Le sketch a aussi été vérifié sur PC avec des interfaces Arduino factices, et dix trames générées ont été parsées comme JSON.

Ces contrôles ne remplacent pas la compilation pour ESP32, l'exécution dans Wokwi ou les mesures de latence. Le kit n'a pas été lancé dans le simulateur pendant sa préparation. Les sorties physiques et les délais restent à vérifier par les essais E01 à E17.

Pour rejouer les tests du moteur sur un PC disposant de g++ : g++ -std=c++11 tests/test_rules.cpp -o rules_tests, puis lancer rules_tests (ou rules_tests.exe sous Windows). Ce n'est pas nécessaire pour commencer dans Wokwi.

## 8. Passer à deux zones

Lorsque E01 à E17 sont compris, duplique le projet Wokwi. Dans le deuxième sketch, remplace ZONE par zone_02 et DEVICE par esp32_02. Garde les mêmes règles et crée des scénarios indépendants. Les deux simulations produiront des mesures distinctes.

Deux fenêtres Wokwi ne constituent pas à elles seules une supervision centralisée. Celle-ci arrive avec MQTT et Node-RED à l'étape suivante.

## 9. Ajouter le serveur sans se bloquer

Mosquitto est le broker : il distribue les messages MQTT. Node-RED reçoit les trames, applique les règles centrales et construit les traitements. FlowFuse Dashboard fournit l'interface web. MongoDB conserve l'historique. Ces logiciels tournent sur ton PC ; Wokwi représente la partie ESP32.

Attention au réseau : la passerelle publique de Wokwi accède à Internet, mais pas directement à ton réseau local. Mettre localhost ou 127.0.0.1 dans le programme de l'ESP32 ne permet donc pas de joindre Mosquitto sur ton PC. La passerelle privée donne cet accès ; la documentation Wokwi la réserve actuellement aux utilisateurs payants. Avec cette passerelle, l'adresse spéciale host.wokwi.internal désigne le PC hôte.

Trois chemins sont possibles pour la suite :

| Chemin | Utilisation | Limite à noter |
| --- | --- | --- |
| Simulation locale gratuite, puis générateur de trames sur PC | Wokwi pour le circuit, script PC pour tester MQTT et le serveur local | Deux validations séparées ; ne pas prétendre que les données du circuit arrivent en direct |
| Broker de démonstration accessible sur Internet | Relier Wokwi et Node-RED pour une intégration réseau d'apprentissage | Données fictives uniquement ; ce montage public ne valide pas la sécurité finale du cahier |
| Passerelle privée Wokwi | Relier directement l'ESP32 simulé à Mosquitto sur le PC | Fonction payante selon la documentation actuelle |

Commence par la première voie pour ne pas dépendre d'un abonnement. Aucun achat n'est nécessaire pour faire les essais locaux du kit.

À l'étape MQTT, conserve les topics définis dans ton document : emergency/demo_01/zone_01/telemetry, events, status, heartbeat, commands et acks. Pour zone_02, change la zone dans le topic. Le firmware final utilisera ESP-IDF/ESP-MQTT pour TLS, les événements QoS 1, les confirmations et le testament MQTT. Aucun topic de commande ne doit être retenu.

N'ajoute pas toutes les fonctions à la fois : valide d'abord une trame reçue, puis la concordance des règles centrales, puis le dashboard, puis l'archivage. Le dashboard final distinguera gravité, types d'incident et santé ; un nœud silencieux ne devient pas NORMAL.

## 10. Fonctions restant à réaliser après ce kit

Le kit couvre l'initiation aux SF01, SF03 (format JSON local), SF04, SF10 (acquittement local), SF11 et à une partie de SF02/SF12/SF14. Le filtre ADC est une moyenne de huit lectures rapprochées ; les rebonds des commandes sont désactivés dans le diagramme. Le traitement temporel du bruit et des rebonds réels reste à caractériser.

Ne sont pas encore réalisés : publication MQTT, broker, règles centrales Node-RED, dashboard, MongoDB, email, authentification, TLS/ACL, heure synchronisée, commandes distantes avec expiration et déduplication, heartbeat, OFFLINE, tampon RAM de 300 trames, reprise historique, sauvegarde et tests de charge de 24 h. Le bouton ACQUITTER est une démonstration locale ; il ne journalise pas encore l'identité d'un superviseur.

Le dossier est une base de départ, pas la validation complète des critères CA01 à CA12. Pour la version entièrement simulée, consigne la dérogation de périmètre avant d'annoncer le projet conforme.

## 11. Si quelque chose ne fonctionne pas

| Symptôme | Vérification |
| --- | --- |
| rules.h introuvable | Créer ce fichier dans Wokwi ; vérifier son nom exact et coller son contenu |
| DHTesp.h introuvable | Ajouter « DHT sensor library for ESPx » avec Library Manager ou libraries.txt |
| Le circuit ne s'affiche pas | Remplacer diagram.json entier, conserver les guillemets et les accolades |
| Le voyant jaune apparaît au démarrage | Attendre le préchauffage simulé de quatre secondes |
| NORMAL n'apparaît pas | SOURCE doit être à droite, DÉFAUT DHT à gauche ; vérifier température, ADC et alarmes mémorisées |
| CRITICAL n'apparaît pas | Vérifier température ≥ 60 °C, mq2_raw ≥ 2500, puis attendre deux acquisitions avec MQ prêt |
| RESET refusé | Corriger les entrées, laisser finir le PIR, attendre trois acquisitions normales ; vérifier la validité du DHT |
| Pas de son | Vérifier le volume de l'onglet et du navigateur ; observer aussi la gravité JSON et le voyant rouge |
| La charge bleue semble inversée | Le code suit le comportement documenté COM→NO lorsque IN est LOW pour le module par défaut ; vérifier l'attribut transistor et le câblage NO/NC |

## 12. Références officielles

- Démarrage ESP32 : https://wokwi.com/projects/new/esp32
- ESP32 dans Wokwi : https://docs.wokwi.com/guides/esp32
- Réseau et passerelles : https://docs.wokwi.com/guides/esp32-wifi
- DHT22 et bibliothèque conseillée : https://docs.wokwi.com/parts/wokwi-dht22
- Bibliothèques : https://docs.wokwi.com/guides/libraries
- Format du circuit : https://docs.wokwi.com/diagram-format
- Potentiomètre : https://docs.wokwi.com/parts/wokwi-potentiometer
- PIR : https://docs.wokwi.com/parts/wokwi-pir-motion-sensor
- Interrupteurs : https://docs.wokwi.com/parts/wokwi-slide-switch
- Relais : https://docs.wokwi.com/parts/wokwi-relay-module

Les instructions de réseau et de composants ont été vérifiées dans la documentation officielle pendant la préparation du guide. Les offres et interfaces de Wokwi peuvent évoluer.
