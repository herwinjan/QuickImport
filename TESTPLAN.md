# Quick Import — Testplan

Testplan voor de wijzigingen van 2026-08-29 (zie CHANGELOG.md). Voer de
tests uit met de app gestart vanuit een terminal (`open build/QuickImport.app`
of direct `build/QuickImport.app/Contents/MacOS/QuickImport`) zodat je de
console-uitvoer ziet — een aantal tests kijkt daarnaar.

**Benodigd testmateriaal:**

- Een SD-kaart met een flinke set foto's (het liefst > 200 MB en > 100
  bestanden), met daarop bij voorkeur RAW+JPEG-paren.
- Indien beschikbaar: een CFexpress-kaart met snelle lezer.
- Indien beschikbaar: een Nikon NEF-bestand (big-endian EXIF).
- Een bestand of map met een accent/speciaal teken in de naam (bijv. `Café`).
- Een grote videofile (> 1 GB) op de kaart voor de annuleer-test.

> Tip: geen kaart bij de hand? Een USB-stick geformatteerd als exFAT/FAT32
> met testbestanden erop wordt ook als "kaart" herkend.

---

## 1. Opstarten & basis

| # | Test | Verwacht resultaat |
|---|---|---|
| 1.1 | Start de app | Venster verschijnt één keer (geen dubbele show/flikkering), titel "Quick Import 0.7.0", about-dialog (tenzij uitgevinkt), daarna kaartselectie |
| 1.2 | Geen kaart aanwezig | Nette melding "No Card found, please insert card." — geen crash |
| 1.3 | Systeemtaal Nederlands | UI in het Nederlands, inclusief de placeholders ("--Locatie niet ingesteld--", "--Selecteer om voorinstelling te laden--", "-- stel projectnaam in --") |

## 2. Kaart laden (parallelle EXIF-parsing)

| # | Test | Verwacht resultaat |
|---|---|---|
| 2.1 | Laad een volle kaart | Merkbaar sneller dan voorheen; statusbalk telt in sprongen van 25 ("loading EXIF data #25 of …") en de GUI blijft responsief tijdens het laden |
| 2.2 | Boomstructuur na laden | Foto's gegroepeerd op jaar → maand → dag → uur volgens de **opnamedatum** (EXIF), niet de bestandsdatum |
| 2.3 | RAW+JPEG-paar | Beide bestanden hangen onder hetzelfde uur. Test óók met een gekopieerde map (bestandsdatum ≠ opnamedatum): het paar mag niet splitsen |
| 2.4 | JPEG selecteren | Preview-overlay toont ISO, diafragma, sluitertijd en cameranaam (komt nu via Exiv2) |
| 2.5 | Nikon NEF (big-endian) | ISO/diafragma/sluitertijd in de overlay zijn realistische waarden (geen absurde getallen) |
| 2.6 | Video's (mov/mp4) | Staan in de boom op bestandsdatum; geen EXIF-warnings die de app hinderen |
| 2.7 | Pad met accent (Café) | Bestanden in zo'n map laden en tonen previews normaal |

## 3. Preview & bladeren

| # | Test | Verwacht resultaat |
|---|---|---|
| 3.1 | Pijl omlaag door een reeks foto's | Previews volgen vlot; snel doorbladeren toont uiteindelijk de láátste selectie (geen oude foto die er overheen komt) |
| 3.2 | Terug-bladeren (pijl omhoog) | Vrijwel instant (komt uit de cache) |
| 3.3 | Vooruit bladeren in rustig tempo | Vrijwel instant (prefetch heeft de volgende al geladen) |
| 3.4 | Kapot/leeg bestand selecteren | Zwart vlak met "Failed to load image." — geen crash |
| 3.5 | Preview-checkbox uit | Geen previews meer; weer aan → previews werken weer |
| 3.6 | Herlaad kaart (⌘R) nadat je op de kaart een foto vervangen hebt (zelfde naam) | De nieuwe versie verschijnt (cache is geleegd bij herladen) |
| 3.7 | Enter / dubbelklik op foto | Fullscreen quick view opent; pijltjes/spatie werken daarna terug in de lijst |
| 3.8 | Snel bladeren direct na kaart-load, daarna app sluiten | Geen crash bij afsluiten (preview-thread stopt netjes) |

## 4. Selectie & teller

| # | Test | Verwacht resultaat |
|---|---|---|
| 4.1 | "Check all" / "Uncheck all" | Alle vinkjes aan/uit; teller en GB-totaal kloppen direct |
| 4.2 | Meerdere rijen selecteren + "Check Selected" / "uncheck Selected" | Alleen die rijen wijzigen; map-rij aanvinken vinkt alle kinderen aan |
| 4.3 | Spatiebalk op selectie | Vinkjes klappen om; eerste item bepaalt de richting voor de hele selectie |
| 4.4 | Vinkje direct in de boom aanklikken (map-niveau) | Alle onderliggende bestanden volgen; teller klopt |
| 4.5 | Importknop | Alleen actief als er selectie is én importlocatie, projectnaam en formaat gezet zijn |

## 5. Import — basis en integriteit

Zet voor deze tests MD5-check **aan**.

| # | Test | Verwacht resultaat |
|---|---|---|
| 5.1 | Importeer een set foto's | Bestanden staan op de juiste plek met de juiste naam (tokens volgen de opnamedatum); MD5 van bron en doel identiek (steekproef: `md5 <bron> <doel>`) |
| 5.2 | Voortgangsbalk | Loopt vloeiend byte-voor-byte mee, ook binnen grote bestanden (springt niet per bestand) |
| 5.3 | Annuleren midden in een groot videobestand | Stopt binnen ~een seconde; **geen** `.part`-bestanden achtergebleven op doel of backup (`find <doel> -name '*.part'`) |
| 5.4 | Cancel-knop nogmaals / na afloop van worker 1 | Geen crash |
| 5.5 | Nogmaals dezelfde import, "delete existing" **uit**, MD5 **aan** | Bestanden tellen als geslaagd (identiek), geen foutmeldingen |
| 5.6 | Nogmaals dezelfde import, "delete existing" **uit**, MD5 **uit** | Foutmelding "Destination already exists …" per bestand; telling "failed" klopt |
| 5.7 | Nogmaals dezelfde import, "delete existing" **aan** | Bestanden worden overschreven, geen fouten |
| 5.8 | Snelheid t.o.v. oude versie (optioneel) | Met MD5 aan hoort de import duidelijk sneller te zijn (kaart wordt maar één keer gelezen) |

## 6. Backup-import (tee-copy)

| # | Test | Verwacht resultaat |
|---|---|---|
| 6.1 | Backup aan, beide locaties gezet, importeer | Bestand staat op **beide** locaties; MD5 van bron, import en backup identiek |
| 6.2 | Kijk tijdens de import naar beide doelmappen | Beide worden tegelijk gevuld (één leespass, tee-write) |
| 6.3 | Backup aan maar geen backupmap gezet | Melding "No backup folder set, please set one first." vóór de import start |
| 6.4 | Backuplocatie een USB-stick, trek 'm eruit vóór de import | Melding "Backup location is not available." |
| 6.5 | Backup te weinig ruimte | Melding "Not enough diskspace available on backup location!" |
| 6.6 | Backup-schijf loopt vól tijdens de import | Die bestanden falen ("Backup failed …"), bron wordt **niet** verwijderd (bij delete-after-import), import stopt niet volledig |

## 7. Delete after import (dataveiligheid!)

⚠️ Test dit eerst met kopieën die je kunt missen.

| # | Test | Verwacht resultaat |
|---|---|---|
| 7.1 | Delete-after-import aan, gewone import | Bron verwijderd, doel compleet en correct |
| 7.2 | Delete-after-import + backup aan, backup laten falen (6.6) | Bron van de gefaalde bestanden staat **nog op de kaart** |
| 7.3 | Import annuleren halverwege | Alleen bestanden die volledig geslaagd zijn, zijn van de kaart verwijderd |

## 8. Adaptieve tweede worker

Console-uitvoer nodig.

| # | Test | Verwacht resultaat |
|---|---|---|
| 8.1 | Import > 200 MB vanaf een **SD-lezer** | Console: "Copy throughput probe: X MB/s"; X < 300 en er verschijnt **geen** "Fast reader detected" |
| 8.2 | Import > 200 MB vanaf een **CFexpress-lezer** (indien beschikbaar) | Probe-regel toont hoge waarde en "Fast reader detected, starting second copy worker"; import verloopt correct en telling klopt |
| 8.3 | Meld de gemeten waarden terug | Als de CFexpress-lezer onder de 300 MB/s meet, stellen we de drempel bij |

## 9. Na-import-acties

| # | Test | Verwacht resultaat |
|---|---|---|
| 9.1 | "Eject after import" | Kaart wordt na een geslaagde import uitgeworpen (⌘E werkt ook los) |
| 9.2 | "Eject if card is empty" + delete-after-import van alles | Kaart wordt uitgeworpen als er niets meer op staat |
| 9.3 | "Quit after import" / "Quit if card is empty" | App sluit op het juiste moment |
| 9.4 | "Open application after import" (bijv. Lightroom/Finder) | Gekozen applicatie opent met de laatste importmap |
| 9.5 | Import mislukt of geannuleerd | Bovenstaande acties gebeuren **niet** |

## 10. Instellingen, presets & vrije ruimte

| # | Test | Verwacht resultaat |
|---|---|---|
| 10.1 | Wijzig alle checkboxes, herstart de app | Alle standen bewaard |
| 10.2 | Preset opslaan, instellingen wijzigen, preset laden | Alle checkbox-standen komen terug |
| 10.3 | Locaties/projectnamen/formaten opslaan en verwijderen | Lijsten en laatst-gebruikt overleven een herstart; verwijderen laat geen rare selectie achter |
| 10.4 | Typ snel in het projectnaamveld | Geen haperingen; voorbeeldpad ("Import to") volgt live |
| 10.5 | Vrije-ruimte-labels | Kloppen; na wisselen van importmap direct ververst; verder maximaal ~3 s oud |
| 10.6 | Bestandsnaamformaat meermaals resetten/legen | Geen dubbele "{J}/{o}"-entries in de lijst |

## 11. Hotplug / kaartdetectie

Dit was voorheen wisselvallig — test dit meerdere keren achter elkaar.

| # | Test | Verwacht resultaat |
|---|---|---|
| 11.1 | Steek een kaart in terwijl de app draait | Vraag "Wilt u de nieuw geplaatste kaart openen?" verschijnt zodra de kaart gemount is — óók als het mounten even duurt. Bij "Yes" opent de kaart direct (geen "No Card found") |
| 11.2 | Herhaal 11.1 vijf keer (uitwerpen, insteken) | Elke keer precies één vraag; nooit nul, nooit twee |
| 11.3 | Kaart insteken en direct weer verwijderen (vóór de vraag) | Geen vraag, geen crash |
| 11.4 | exFAT-kaart > 32 GB (GUID-partitieschema) insteken | Precies één vraag (geen dubbele van schijf + partitie) |
| 11.5 | CFexpress-lezer (Thunderbolt/PCIe) insteken, indien beschikbaar | Vraag verschijnt nu ook (werd voorheen nooit gedetecteerd) |
| 11.6 | Niet-kaart-media: USB-stick met APFS/HFS+, of een DMG mounten | Géén vraag (verkeerd bestandssysteem wordt genegeerd) |
| 11.7 | Kaart die al geladen is nogmaals — trek de lezer eruit en steek 'm terug | Vraag verschijnt (kaart was ontladen); een al gelaten kaart opnieuw aanbieden terwijl hij geladen is geeft géén vraag |
| 11.8 | App starten met kaart al in de lezer | Geen extra vraag bovenop de normale kaartselectie bij het opstarten |
| 11.9 | Trek de geladen kaart eruit | Venster leegt netjes ("No card loaded."), geen crash |
| 11.10 | Antwoord "No" op de vraag | Geen herhaalde vraag voor dezelfde kaart; volgende insteek-actie vraagt wel weer |

---

## Afmelden

Noteer per sectie ✅/❌ plus bij ❌ de console-uitvoer. De belangrijkste
secties zijn **5, 6 en 7** (data-integriteit) — als die groen zijn, is de
kern in orde. Sectie 8.3 (gemeten throughput) bepaalt of de
tweede-worker-drempel goed staat.
