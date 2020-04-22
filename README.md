- zgodnie z odpowiedzią na pytanie Q35*, nie ufam długości podanej przy “Content-length” i jeśli wiadomość nie jest chunked to pobieram ją całą i wypisuję jej rozmiar
- zgodnie z odpowiedzią na pytanie Q30**, w raporcie wypisuję wszystkie cookies z pliku podanego jako argument oraz te nowe, otrzymane od serwera, pomijając powtórzenia (to jest, mogą się pojawić w raporcie jednocześnie ciastka o tej samej nazwie, lecz innej wartości np. wiersze “ciastko=wartość” oraz “ciastko=“, ponieważ pomimo tej samej nazwy, mają różne wartości)


*[Q35] Czy należy pobrać message-body, gdy ustawiony jest nagłówek content-length?
[A35] Tak. Reczywista długość może być inna niż ta zaraportowana w Content-length.

**[Q30] Czy wypisać mamy wszystkie cookies, te z pliku "ciasteczka.txt" oraz nowe, oczywiście pomijając powtórzenia?
[A30] Tak.
