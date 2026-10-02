# Blip

En sfære-synth: 14 lyde ligger på en kugle. Træk i kuglen, og de lyde der vender mod dig (trådkorset) bliver blandet sammen. Jo tættere, jo mere fylder de.

VST3 og AU til Mac (Apple Silicon og Intel). Bygget med JUCE 8.

## Sådan bruges den

- **Træk i kuglen** for at dreje den. **Klik på et navn** for at glide derhen.
- **Kegle** bestemmer hvor bredt du lytter: lav = én lyd ad gangen, høj = mange lyde blandet.
- **Modhjulet** (CC1) på dit keyboard drejer kuglen.
- Kuglens drejning er to parametre (*Drej vandret* og *Drej lodret*), så den kan automatiseres i Ableton.
- Dobbeltklik på en knap for at nulstille den.

## De 14 lyde

Varm pad · Glas klokke · Sav bas · FM bjælde · Vind støj · Pluk · Sub bas · Kor · Orgel · PWM lead · Granulær · Chip arp · Mørk drone · Strenge

Alle laves direkte i koden (ingen samples) i `Source/BlipVoice.h`.

## Byg den

Hver gang der pushes til `main`, bygger GitHub den automatisk under **Actions**. Hent zip-filen **Blip-mac** nederst på bygge-siden, og følg `INSTALLER.txt`.

Lokalt på en Mac (kræver Xcode og CMake):

```
cmake -B build -DCMAKE_BUILD_TYPE=Release
cmake --build build --target Blip_VST3 Blip_AU
```

## Filer

- `Source/SphereModel.h` – kuglen: placering af lyde, drejning og blanding
- `Source/BlipVoice.h` – lydmotorerne, filter, envelope og glide
- `Source/PluginProcessor.*` – parametre og lyd
- `Source/PluginEditor.*` – vinduet (logo, kugle, knapper)
- `Resources/` – skrifttypen IBM Plex Mono (SIL Open Font License)
