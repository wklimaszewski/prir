#include <iostream>
#include <math.h>

using namespace std;

int main()
{
    int n = 0;
    float wynik = 0, liczba = 0;
    cout << "Podaj liczbe " << endl;
    cin >> n;

    for( int i = 0; i <= n; i++ )
    {
        liczba =( pow( - 1.0, i ) ) /( 2 * i + 1 );
        cout << "Dla i= " << i << "   liczba=" << liczba << endl;;
        cout << "Dla i=" << i << "  wynik+liczba   " << wynik << " + " << liczba;
        wynik += liczba;
        cout << " = " << wynik << endl << endl;

    }
    return 0;
}
