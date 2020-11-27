#include <iostream>
#include <bitset>
#include <stdint.h>
#include <vector>
#include <fstream>
#include <sys/stat.h>

#include "tablice.hpp"
#include "tablice.cpp"

using namespace std;

long odczytajRozmiar(vector<uint64_t> bloki)
{
    long rozmiar;
    uint8_t wolne = (uint8_t)bloki.back();
    rozmiar = bloki.size() * 8l - wolne;
    return rozmiar;
}

void dodajOstatniBajt(vector<uint64_t> &bloki, long rozmiar)
{
    if (rozmiar % 8 == 0)
        bloki.push_back(0ull);
    bloki.back() |= (8l * bloki.size()) % rozmiar;
}

vector<uint64_t> podzielNaBloki(const char* zawartosc, long rozmiar)
{
    vector<uint64_t> bloki;
    uint64_t blok = 0ull;
    int i = 0, j;

    while (i < rozmiar)
    {
        for (j = 1; j <= 8; j++)
        {
            blok |= (uint64_t)((uint8_t)zawartosc[i]) << (64 - 8 * j);
            i++;
            if (i == rozmiar)
            	break;
        }
        bloki.push_back(blok);
        blok = 0ull;
    }
    return bloki;
}

uint32_t przesun(uint32_t blok, int rozmiarBloku, int przesuniecie)
{
    bool bit;
    for (int i = 0; i < przesuniecie; i++)
    {
        bit = blok >> rozmiarBloku - 1;
        blok <<= 1;
        blok &= (~0ul >> (32 - rozmiarBloku));
        blok |= bit;
    }
    return blok;
}

uint64_t permutuj(uint64_t blok, const uint8_t permutacja[], int rozmiarDocelowy, int rozmiarPierwotny)
{
    uint64_t blokPermutowany = 0;
    bool bit;
    for (int i = 0; i < rozmiarDocelowy; i++)
    {
        bit = blok & (1ull << (rozmiarPierwotny - permutacja[i]));
        blokPermutowany |= (uint64_t)bit << (rozmiarDocelowy - i - 1);
    }
    return blokPermutowany;
}

void generujPodklucze(uint64_t klucz, uint64_t podklucze[])
{
    uint32_t kluczL, kluczP;
    klucz = permutuj(klucz, Tablice::PERMUTOWANY_WYBOR_1, 56, 64);

    kluczL = (uint32_t)((klucz & (~0ull << 28)) >> 28);
    kluczP = (uint32_t)(klucz & (~0ull >> (64 - 28)));

    for (int i = 0; i < 16; i++)
    {
        kluczL = przesun(kluczL, 28, Tablice::PRZESUNIECIA[i]);
        kluczP = przesun(kluczP, 28, Tablice::PRZESUNIECIA[i]);
        podklucze[i] = kluczP | ((uint64_t)kluczL << 28);
        podklucze[i] = permutuj(podklucze[i], Tablice::PERMUTOWANY_WYBOR_2, 48, 56);
    }
}

uint8_t s(uint8_t blok, const uint8_t s[][16])
{
    uint8_t wiersz, kolumna;
    bool pierwszyBit, ostatniBit;

    pierwszyBit = blok >> 6 - 1;
    ostatniBit = blok & 1u;

    wiersz = ((uint8_t)pierwszyBit << 1) | ostatniBit;
    blok >>= 1;
    kolumna = blok & (~0u >> (32 - 4));

    return s[wiersz][kolumna];
}

uint32_t funkcja(uint64_t blok, uint64_t podklucz)
{
    uint32_t blokS = 0ull;

    blok = permutuj(blok, Tablice::TABLICA_ROZSZERZENIA, 48, 32);
    blok ^= podklucz;

    for (int j = 0; j < 8; j++)
    {
        uint8_t grupa = blok & (~0ull >> (64 - 6));
        blok >>= 6;
        blokS |= s(grupa, Tablice::S[8 - j - 1]) << (j * 4);
    }

    blokS = permutuj(blokS, Tablice::PERMUTACJA_P, 32, 32);
    return blokS;
}

uint64_t des(uint64_t blok, uint64_t klucz, bool szyfruj)
{
    uint64_t podklucze[16];
    uint32_t blokL, blokP;

    generujPodklucze(klucz, podklucze);

    blok = permutuj(blok, Tablice::PERMUTACJA_POCZATKOWA, 64, 64);

    blokL = (uint32_t)(blok >> 32);
    blokP = (uint32_t)(blok);

    for (int i = 0; i < 16; i++)
    {
        int indeks = szyfruj ? i : 16 - i  - 1;

        uint32_t poprzedniBlokP = blokP, poprzedniBlokL = blokL;
        blokL = poprzedniBlokP;
        blokP = poprzedniBlokL ^ funkcja(poprzedniBlokP, podklucze[indeks]);
    }
    blok = (uint64_t)blokP << 32 | blokL;
    blok = permutuj(blok, Tablice::PERMUTACJA_POCZATKOWA_T, 64, 64);
    return blok;
}

void zapiszBloki(vector<uint64_t> bloki, string nazwaPliku, long rozmiar)
{
    ofstream wyjscie(nazwaPliku, ios::out | ios::binary);
    char znak;
    long i = 0;
    for (uint64_t blok : bloki)
    {
        for (int j = 1; j <= 8; j++)
        {
        	if (i >= rozmiar)
                break;
            znak = ((0xFFull << (64 - 8 * j)) & blok) >> (64 - 8 * j);
            wyjscie.put(znak);
            i++;
        }
    }
    wyjscie.close();
}

vector<uint64_t> wczytajBloki(string nazwaPliku, long &rozmiar)
{
    ifstream wejscie(nazwaPliku, ios::in | ios::binary);
    struct stat info;
    char* bufor;
    vector<uint64_t> bloki;
    if (wejscie)
    {
        stat(nazwaPliku.c_str(), &info);
        rozmiar = info.st_size;
        bufor = new char[rozmiar];
        wejscie.read(bufor, rozmiar);
        wejscie.close();
        bloki = podzielNaBloki(bufor, rozmiar);        	
        delete [] bufor;
        return bloki;
    }
    else
    {
        cout << "Blad otwarcia pliku:" << ' ' << strerror(errno) << endl;
        wejscie.close();
        return vector<uint64_t>();
    }
}

int main()
{
    vector<uint64_t> bloki, przetworzoneBloki;
    uint64_t klucz;
    string nazwaPliku, kluczTekst;
    long rozmiar;
    char odpowiedz;
    bool szyfruj = false;

    while (odpowiedz != 'a' && odpowiedz != 'b')
    {
    	cout << "Wybierz operacje:" << endl;
    	cout << "a) szyfrowanie" << endl;
    	cout << "b) odszyfrowanie" << endl;
    	cin >> odpowiedz;
	}
	if (odpowiedz == 'a')
		szyfruj = true;

    cout << "Podaj nazwe pliku:" << endl;
    cin >> nazwaPliku;

    cout << "Wczytywanie..." << endl;
    bloki = wczytajBloki(nazwaPliku, rozmiar);
    cout << "Zakonczono wczytywanie." << endl;

    if (szyfruj)
        dodajOstatniBajt(bloki, rozmiar);
	
    cout << "Podaj klucz:" << endl;
    cin >> kluczTekst;
    klucz = podzielNaBloki(kluczTekst.c_str(), kluczTekst.length() < 8 ? kluczTekst.length() : 8)[0];

    cout << "Przetwarzanie..." << endl;
    for (uint64_t blok : bloki)
    {
        uint64_t przetworzonyBlok = des(blok, klucz, szyfruj);
        przetworzoneBloki.insert(przetworzoneBloki.begin(), przetworzonyBlok);
    }
    
    if (!szyfruj)
    	rozmiar = odczytajRozmiar(przetworzoneBloki);
    else
    	rozmiar = przetworzoneBloki.size() * 8l;

    zapiszBloki(przetworzoneBloki, "wynik_" + nazwaPliku, rozmiar);
    cout << "Zakonczono." << endl;

    system("pause");
    return 0;
}
