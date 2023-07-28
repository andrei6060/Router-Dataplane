#Constantinescu Andrei 324CD Tema 1 Protocoale de Comunicatii

##IPV4

* Pentru a implementa partea de IPV4 a temei, la inceput am folosit un pointer de tip ether_header, si unul de tip iphdr, din buffer-ul primit. In continuare am verificat daca tipul din cadrul header-ului ethernet este 0x0800, ceea ce reprezinta un pachet tip IPV4. In caz afirtmativ, verific checksum-ul primit, iar apoi, in cazul in care IP-ul destinatie nu este al router-ului, scad TTL-ul pachetului. Daca acest TTL este indeajuns de mare, caut urmatorul hop pe care trebuie trimis pachetul si il trimit.


##LPM Optim

* Am implementat o cautare binara pentru a gasi urmatorul hop pe care trebuie trimis pachetul. In cadrul acestei implementari am folosit functia qsort din stdlib.h pentru a sorta tabela de rutare. Pentru a folosi aceasta functie, a trebuit sa implementez o functie de comparare. Aceasta functie compara prefixurile a 2 intrari din tabela de rutare, iar daca aceseta coincid, compara mastile celor doua intrari. Dupa sortare, in cadrul cautarii binare caut intrare din tabla de rutare ce are prefixul egal cu rezultatul operatiei de "AND" pe biti dintre masca respectivei intrari si IP-ul destinatie. 

##ARP

* Pentru acest task am renuntat la tabela ARP statica si a trebuit sa folosesc una dinamica. Din aceasta cauza, in timpul transmiterii pachetelor, se poate intalni o destinatie a carei adresa MAC sa fie necunoscuta. Pentru a rezolva aceasta problema am implementat functionalitatile pentru ARP Reply si ARP Request. Astfel, routerul trimite un request in retea, la care raspunde doar entitatea cu IP-ul ce are MAC-ul cautat, facandu-si si adresa MAC cunoscuta. Apoi trimit pachetul ce se afla in coada.

##ICMP

* Acest protocol il folosesc pentru a trimite un raspuns in cazul in care pachetul de tip IPV4 primeste un pachet cu TTL-ul prea mic, in cazul in care destinatia este cea a router-ului sau in cazul in care nu e nicio intrare in tabela de routare care sa ajute la transmitera pachetului. In toate cazurile pachetul trimis are aceeasi structura, mai putin tipul din cadrul header-ului de tip ICMP. Acest camp are valoarea 3 (ruta buna inexistenta), 11 (TTL prea mic) sau 0 (adresa IP destinatie coincide cu cea a router-ului)
