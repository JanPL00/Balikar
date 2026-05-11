# Testy Balikaru

## Unit testy

Unit testy jsou napsány pomocí frameworku [Check](https://libcheck.github.io/check/) a pokrývají
moduly: `system_helpers`, `cli_helpers`, `parser`, `solver`, `downloader`, `builder`, `installer`.

### Sestavení a spuštění

**Předpoklad:** sestavený projekt (viz kořenové README.md, sekce Sestavení).

```bash
# Z kořenového adresáře projektu:
cmake -B build
cmake --build build

# Spustit testovací binárku:
./build/tests
```

Výstup zobrazí výsledek každé testovací sady (suite). Při selhání CTest vypíše detaily
díky `--output-on-failure`.

### Struktura testů

```
tests/
  main.c               – Vstupní bod, spouští všechny suites
  tests-src/           – Testy hlavních modulů (builder, downloader, installer, parser, solver)
  tests-utils/         – Testy pomocných modulů (cli_helpers, system_helpers)
```

---

## Docker testy

Integrační testy v izolovaném prostředí jsou zdokumentovány v
[tests/docker-tests/README.md](docker-tests/README.md).

Zahrnují tři scénáře: základní workflow (install, build, clean-cache),
pokročilé příkazy a reálné balíčky z LFS repozitáře.


**Požadavek:** Docker 20.10+.
