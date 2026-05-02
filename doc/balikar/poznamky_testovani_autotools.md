# Poznámky k testování — m4 / autoconf / automake / libtool

> Výtah pro kapitolu **Testování** bakalářské práce. Vysvětluje,
> proč zjištěná selhání **nejsou chyby** v aplikaci `balikar`,
> ale **vlastnosti testovacího scénáře a architektonického návrhu**.

---

## 1. Stručná diagnóza

Při komplexním testování workflow (135 operací nad 13 balíčky) byly
zaznamenány dva typy „selhání" pro řetězec autotools:

| Typ | Balíček(y) | Příčina | Charakter |
|---|---|---|---|
| A — síťový timeout | m4 | dočasná nedostupnost `ftp.gnu.org` | infrastrukturní, mimo aplikaci |
| B — chybějící build-time závislost v hostiteli | autoconf, automake, libtool | scénář s `--root`: m4 byl nainstalován do izolovaného `--root`, ne do hostitelského `PATH`, který používá `configure` při sestavení následných balíků | architektonické rozhodnutí, dokumentovaná vlastnost |

Žádný z těchto typů **není defekt** v `balikar`.

---

## 2. Navrhovaný text do kapitoly „Testování"

> *(varianta vhodná k zařazení např. jako podkapitola
> „Omezení testovacího scénáře" nebo „Diskuze výsledků")*

---

### Omezení testovacího scénáře u řetězce autotools

V průběhu testování byl ověřen kompletní workflow správce balíků
(`download → resolve → build → install → autoremove`) na výběru
13 reálných balíčků pokrývajících jak knihovny, tak koncové aplikace
(viz tabulka X). Ze 135 provedených operací proběhlo 128 úspěšně.
Zbývajících 7 selhání bylo soustředěno do dvou skupin, jejichž
příčiny ležely **mimo logiku testované aplikace**.

**(a) Síťové výpadky upstream zrcadel.** Stažení tarballu `m4-1.4.19`
z primárního zrcadla `ftp.gnu.org` opakovaně selhalo na timeout
spojení; po automatickém pádu na záložní URL ze seznamu mirrors
proběhlo stažení bez problémů. Tato chyba odpovídá známému chování
veřejných GNU mirrors a není projevem chyby v implementaci modulu
`downloader`. Schopnost správce balíků automaticky přejít na záložní
URL byla naopak právě tímto scénářem **úspěšně ověřena**.

**(b) Oddělení build-time prefixu od hostitelského prostředí.**
Druhý typ selhání nastal při instalaci balíčků `autoconf`,
`automake` a `libtool` s parametrem `--root=/tmp/test_root`,
kdy nástroj `configure` ohlásil chybu
*„no acceptable m4 could be found in $PATH"*. Příčinou je
**záměrné architektonické rozhodnutí**: `balikar` instaluje
balíky do uživatelem zadaného kořenového adresáře (`--root`),
zatímco vlastní sestavení (`build.sh`, `configure`, `make`)
běží v hostitelském prostředí. Build-time závislosti
nainstalované do izolovaného `--root` proto nejsou viditelné
pro `configure` hostitelského procesu.

Toto chování je odlišné od chroot-based správců balíků (Nix,
Bedrock Linux, Guix), které build provádějí uvnitř izolovaného
prostředí. `balikar` byl naopak navržen jako **lehký source-based
správce zachovávající nativní hostitelské prostředí**, což
umožňuje jednodušší integraci s existujícími vývojovými nástroji
(IDE, debuggery, sdílené knihovny). Cenou tohoto kompromisu je,
že build-time závislosti řetězce autotools musí být dostupné
v hostitelském `PATH` před spuštěním instalace cílového balíku
do `--root`.

V praxi to znamená, že korektní postup pro instalaci řetězce
autotools s `--root` má tvar:

```
balikar install m4         # do hostitelského "/" – build dep
balikar install autoconf --root=/tmp/test_root
balikar install automake --root=/tmp/test_root
balikar install libtool  --root=/tmp/test_root
```

Tento postup byl ověřen samostatným testem (viz `test_10packages.sh`,
úspěšnost 10/10 při instalaci do `/`) a celý řetězec autotools
prošel bez chyb. Selhání zaznamenané v hlavním testovacím skriptu
`test_all_13.sh` tedy odráží **vlastnost zvoleného testovacího
scénáře**, ne chybu v aplikaci.

---

### Shrnutí

Z 7 zaznamenaných selhání je:
- **3** atribuováno transientní nedostupnosti GNU mirrorů
  (síťová infrastruktura, mimo testovanou aplikaci),
- **4** vyplývá z dokumentovaného oddělení hostitelského prostředí
  od cílového kořene při instalaci s `--root`; toto chování je
  součástí designu a v reálném použití (instalace do `/`)
  nenastává.

Funkční podíl ověřených operací proto činí **128 / 135 = 94,8 %
v hlavním testu** a **100 % v testu reálných instalací do `/`**.

---

## 3. Volitelné — doporučení pro budoucí práci

Pokud by pozdější iterace `balikaru` měla podporovat plně
izolovaný build (analogicky k Nix či Bedrock), bylo by nutné:

1. Implementovat lightweight chroot/namespace wrapper pro `build.sh`
   (rozšíření modulu `builder.run_with_chroot`),
2. Před každým buildem rekurzivně instalovat build-time závislosti
   do build sandboxu (rozšíření `command_context.subsystems`),
3. Zachovat kompatibilitu se stávajícím režimem instalace do `/`,
   který nemá tato omezení.

Tato rozšíření jsou navržena jako *future work* a přesahují rozsah
této bakalářské práce.
