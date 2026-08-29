# QuickImport — Uitleg van de code

Dit document beschrijft hoe QuickImport intern werkt: de flow van kaart tot
geïmporteerd bestand, de belangrijkste klassen, het threading-model en de
plekken waar je moet oppassen. (Repo-brede documentatie in het Engels staat in
CLAUDE.md; openstaande punten in TODO.md; wijzigingen in CHANGELOG.md.)

## Wat de app doet

1. Detecteert een geheugenkaart (FAT/exFAT-volume).
2. Scant de kaart recursief op RAW-, JPEG- en videobestanden.
3. Toont die in een boom (jaar → maand → dag → uur → bestand) met preview.
4. Kopieert de selectie naar een projectmap (en optioneel een backup-locatie),
   met hernoemen via tokens en optionele MD5-verificatie.
5. Optioneel daarna: bron verwijderen, kaart uitwerpen, applicatie openen,
   afsluiten.

## De flow, stap voor stap

### 1. Opstarten (main.cpp → MainWindow)

`main.cpp` zet applicatienaam/versie/organisatie, laadt de vertaling uit
`:/translation`, maakt het hoofdvenster en start een `QDeviceWatcher`
(third-party, in `qdevicewatcher/`) die hotplug-events geeft. Belangrijk
detail: het disk-event komt vóórdat macOS het volume gemount heeft;
`slotDeviceAdded` pollt daarom (max ~10 s, via `waitForVolumeMount`) tot de
mount er is en stelt pas dan de "kaart openen?"-vraag — en alleen voor
schrijfbare FAT/exFAT-volumes die niet al geladen zijn. De macOS-backend
filtert partitieschema-containers weg en accepteert naast USB/SD ook
removable/ejectable media (CFexpress-lezers). De constructor
van `MainWindow` laadt alle instellingen uit `QSettings`, verbindt de
signalen van het device-widget en de sneltoetsen (⌘I importeren, ⌘S kaart
kiezen, ⌘E uitwerpen, ⌘R herladen), en opent na de eventloop-start meteen de
kaartselectie (`on_selectCard_clicked`).

### 2. Kaart kiezen (on_selectCard_clicked)

Loopt alle gemounte volumes af (`QStorageInfo::mountedVolumes`) en filtert op
schrijfbare FAT/exFAT/vfat/msdos-volumes — dat zijn vrijwel altijd
geheugenkaarten. Bij één kandidaat wordt die direct gekozen; bij meerdere
verschijnt `SelectCardDialog`.

### 3. Kaart inlezen (reloadCard → FileInfoModel)

`getFileListFromDir()` zoekt recursief (zonder symlinks te volgen) naar
bestanden die matchen met een lange lijst extensies (RAW-formaten, jpg, mov,
mp4, …). De lijst gaat naar `FileInfoModel`, een `QAbstractItemModel` die de
boom **op een achtergrondthread** bouwt via `QtConcurrent::run`:

- Per bestand worden jaar/maand/dag/uur-knopen aangemaakt
  (`findOrCreateTreeNode`) en een `TreeNode` voor het bestand zelf.
- Per bestand opent LibRaw het bestand met een EXIF-callback
  (`exif_callback`) die ISO, sluitertijd, diafragma, camera, serienummer,
  lens, enz. in `imageInfoStruct` zet. Kan LibRaw het bestand niet openen
  (JPEG/HEIC/TIFF), dan leest **Exiv2** dezelfde EXIF-velden
  (`parseExifWithExiv2`) — zo krijgen RAW+JPEG-paren altijd dezelfde
  opnamedatum en splitsen ze nooit over mappen. Video's hebben geen EXIF en
  gebruiken de bestandsdatum. **Let op:** EXIF kan little- of
  big-endian zijn; alle numerieke reads moeten via de helpers
  `readExifU16/U32/URational` die de `ord`-parameter respecteren.
- Voortgang gaat via `updateProcessStatus` naar de statusbalk (thread-safe
  met `QMetaObject::invokeMethod` + `QPointer`).
- Als de future klaar is, swapt `onTreeBuildingFinished()` de nieuwe root in
  met `beginResetModel`/`endResetModel`.

`TreeNode` bezit zijn kinderen (`qDeleteAll` in de destructor); de hele boom
wordt dus opgeruimd door de root te deleten.

### 4. Preview (selectedNode → imageLoader)

Er draait één **persistente** `imageLoader` op een eigen thread voor de
levensduur van het venster. `MainWindow::selectedNode` vraagt previews aan
via een thread-safe API met "laatste verzoek wint"-coalescing; resultaten
komen terug met het pad erbij, zodat verouderde resultaten genegeerd worden
(`previewLoaded`). De loader heeft een **LRU-cache** (~100 MB, zo'n 25
beelden) en **prefetcht** de buurfoto's van de selectie, waardoor bladeren
met de pijltjes vrijwel instant is; de cache wordt geleegd bij het herladen
van de kaart. Voor formaten die `QImageReader` kent wordt geschaald gelezen
(max 1024px); voor RAW haalt LibRaw de ingebedde JPEG-thumbnail op.
`showImage()` tekent de EXIF-gegevens (bestandsnaam, diafragma, sluitertijd,
ISO, brandpuntsafstand, camera) over de afbeelding heen. Enter of dubbelklik
opent een borderless fullscreen-preview (`BorderlessDialog`) die
pijltjes/spatie terugstuurt naar de boom.

### 5. Bestandsnaam-tokens (processNewFileName)

`fileCopyWorker::processNewFileName()` (statisch, ook gebruikt voor het
voorbeeldlabel in het hoofdvenster) vervangt tokens in projectnaam en
bestandsnaamformaat. De datum/tijd-tokens gebruiken de **opnamedatum**
(EXIF `DateTimeOriginal`) als die beschikbaar is, anders de
bestandsdatum — via `fileCopyWorker::captureTimestamp()`, dezelfde regel
als de boomgroepering:

| Token | Betekenis | Token | Betekenis |
|---|---|---|---|
| `{Y}` / `{y}` | jaar (4/2 cijfers) | `{i}` | ISO-waarde |
| `{m}` | maand | `{T}` | cameranaam |
| `{D}` | dag | `{c}` | serienummer camera |
| `{W}` | weeknummer | `{O}` | eigenaar (owner name) |
| `{H}` / `{h}` | uur | `{o}` | originele bestandsnaam |
| `{M}` | minuten | `{r}` | volgnummer uit bestandsnaam |
| `{J}` | opgeloste projectnaam | `{e}` | extensie (met punt) |

Het resultaat mag `/` bevatten; mappen worden aangemaakt met `mkpath`. De
functie geeft `[bestandsnaam, map, volledig pad]` terug.

### 6. Kopiëren (fileCopyDialog → fileCopyWorker)

`on_moveButton_clicked` valideert eerst: importmap gezet, genoeg vrije
schijfruimte op project- én backuplocatie, en er is een selectie. Daarna
opent `fileCopyDialog`, die het werk verdeelt:

- Alle workers halen bestanden uit één gedeelde, thread-safe wachtrij
  (`fileCopyQueue`, atomaire index). De import start altijd met **één
  worker**; na ~200 MB meet de dialog de doorvoer en start alleen bij een
  snelle lezer (≥ ~300 MB/s kaart-reads, geschat; bij MD5 wordt de dubbele
  bron-read meegerekend) een **tweede worker** die uit dezelfde wachtrij
  meepakt. Trage SD-lezers blijven zo single-threaded (kaartvriendelijker en
  meestal sneller), snelle CFexpress-lezers benutten wel twee threads.
  Uitzondering: zijn er **dubbele doelpaden**, dan blijft het altijd één
  worker (twee workers zouden anders naar hetzelfde doelbestand kunnen
  schrijven). De check op de import-bestemming dekt ook de backup: het
  naamformaat is gelijk, alleen de map-prefix verschilt.
- De copy zelf is defensief opgezet (dit is de kern van de app, wees hier
  voorzichtig) en leest de kaart **precies één keer** per bestand: een
  chunked leesloop (8 MB) schrijft naar een **tempbestand**
  (`<doel>.quickimport.<uuid>.part`) op de importlocatie én — als backup
  aanstaat — tegelijk naar een tempbestand op de backuplocatie (tee-copy),
  terwijl de MD5-hash van de bron tijdens het lezen wordt opgebouwd. Daarna
  wordt elk tempbestand op zijn eigen schijf geverifieerd (grootte +
  optionele MD5 tegen de bron-hash) en pas dan atomair ge-`rename`d naar de
  definitieve naam. Annuleren werkt per chunk, dus ook midden in een groot
  bestand. Staat *delete after import* aan, dan wordt de temp-file vóór de
  rename fysiek naar schijf geflusht (`F_FULLFSYNC` op macOS) — de bron mag
  pas weg als de kopie écht op schijf staat. De voortgangsbalk loopt op
  byte-niveau mee (signaal `bytesAccounted`). Bestaat het doel
  al en staat "delete existing" uit, dan faalt de copy — tenzij MD5 aan
  staat en bron en doel identiek blijken (dan telt hij als geslaagd).
- **Bron verwijderen** (delete after import) gebeurt alleen als de copy —
  en, indien ingeschakeld, óók de backup-copy — geverifieerd gelukt is.
- Voortgang per worker wordt in de dialog geaggregeerd
  (`handleProgressFromWorker`). Annuleren zet een `std::atomic_bool`; de
  workers stoppen na het lopende bestand. De dialog sluit pas als alle
  workers klaar én alle threads gestopt zijn (`finalizeIfReady`), met
  `accept()` bij succes en `reject()` bij annulering.

Let op: threads/workers ruimen zichzelf op via `deleteLater` en de pointers
`m_worker`/`m_thread` worden asynchroon genull'd — altijd null-checken.

### 7. Na de import

Afhankelijk van de instellingen: kaart uitwerpen (`diskutil unmountDisk`,
macOS-only), de gekozen applicatie openen met de laatste importmap als
argument, en/of afsluiten (direct, of alleen als de kaart leeg is).

## Instellingen en presets

Alles staat in `QSettings("HJ Steehouwer", "QuickImport")`:

- Losse booleans per checkbox (md5Check, ejectAfterImport, …), direct
  opgeslagen in de `on_*_stateChanged`-slots.
- Lijsten met recent gebruikte importlocaties, backuplocaties, projectnamen
  en bestandsnaamformaten, elk met een "last used"-index.
- Presets (combinaties van checkbox-instellingen) als JSON-blob onder
  `presetSettings`, beheerd via `presetDialog`/`presetListModel`.

## Build in het kort

```sh
cmake -S . -B build \
    -DLIBRAW_ROOT=/Users/herwin/devel/LibRaw \
    -DEXIV2_ROOT=/Users/herwin/devel/exiv2
cmake --build build -j 8
```

Deployment target is macOS 14.0. Gebruik daarom de eigen LibRaw/Exiv2-builds
(voor 14.0 gecompileerd) en niet die van Homebrew (alleen voor jouw huidige
macOS gebouwd). LibRaw wordt dynamisch gelinkt vanuit
`~/devel/LibRaw/lib/.libs/` — voor distributie moet die dylib mee de bundle
in (macdeployqt). Exiv2 is statisch gelinkt en wordt nu alleen door
`xmpengine.cpp` gebruikt.
