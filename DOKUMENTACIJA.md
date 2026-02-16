# Survive - Arena Shooter

## Projektna dokumentacija


# 1. Uvod

Survive je igra tipa arena shooter razvijena u programskom jeziku C++ sa third-person perspektivom. Igrac preuzima ulogu borca koji se nalazi u zatvorenoj areni nalik kolosseumu i mora da prezivi tri minuta neprekidnih napada neprijatelja koji dolaze u talasima kroz cetiri kapije rasporedene po obodu mape. Igra se zavrsava pobedom ako igrac prezivi svih 180 sekundi, ili porazom ukoliko mu zdravlje padne na nulu.

Za renderovanje 3D scene koriscen je Irrlicht Engine verzije 1.8, koji pruza podrsku za ucitavanje MD2 modela (format poznat iz igre Quake 2), upravljanje kamerom, GUI elemente i 2D iscrtavanje. Fizika igre oslanja se na Bullet Physics 3.x biblioteku koja simulira kolizije izmedju igraca, neprijatelja, prepreka i granica arene. Zvucni efekti reprodukuju se pomocu IrrKlang biblioteke koja podrzava formate poput MP3 i WAV.

Projekat je razvijen u Visual Studio 2022 okruzenju, sa konfiguracijom za x64 platformu. Sve zavisnosti (Irrlicht, IrrKlang, Bullet Physics) nalaze se u lib direktorijumu projekta, dok se modeli, teksture, mape i zvukovi nalaze u assets direktorijumu.


# 2. Arhitektura projekta

Projekat je organizovan po principu razdvajanja odgovornosti, gde svaka klasa pokriva jednu jasno definisanu funkcionalnost. Ulazna tacka programa je main.cpp fajl koji kreira instancu klase Game i poziva njenu metodu run().

Klasa Game predstavlja centralni deo aplikacije. Ona inicijalizuje Irrlicht uredjaj, postavlja scenu, kreira fizicki svet, ucitava resurse i pokrece glavni petlju igre. Stanje igre modelovano je konacnim automatom sa sest mogucih stanja: MENU, PLAYING, PAUSED, CREDITS, GAMEOVER i WIN. Tranzicije izmedju stanja kontrolisu se korisnickim akcijama kao sto su klik na dugme u meniju ili pritisak na taster ESC tokom igre.

Klasa GameObject sluzi kao bazna klasa za sve objekte u svetu igre. Ona uparuje Irrlicht scene node (vizuelnu reprezentaciju) sa Bullet rigid body objektom (fizicku reprezentaciju) i pruza metode za sinhronizaciju pozicije izmedju ova dva sistema. Svaki GameObject ima status zivota i mogucnost oznacavanja za uklanjanje iz scene.

Klasa Player nasledjuje GameObject i implementira logiku igraca. Ona upravlja kretanjem na osnovu WASD tastera, pucanjem pomocu raycasta kroz fizicki svet, animacijama MD2 modela i zvucnim efektima. Igrac poseduje zdravlje (pocetno 100), municiju (pocetno 30 metaka) i oruzje koje se vizuelno prikazuje kao zaseban MD2 model zakacen za model igraca.

Klasa Enemy takodje nasledjuje GameObject i implementira vestacku inteligenciju neprijatelja. Svaki neprijatelj prolazi kroz niz stanja: pojavljuje se na kapiji (SPAWNING), hoda ka areni, pozdravlja (SALUTING), prelazi u stanje mirovanja (IDLE), juri igraca (CHASE), ceka red za napad (WAIT_ATTACK), napada (ATTACK) ili umire (DEAD). Neprijatelji koriste dinamicke Bullet kapsule za koliziju i imaju ghost objekat kao triger za detekciju napada.

Klasa Pickup upravlja predmetima za skupljanje, konkretno paketima municije. Svaki pickup koristi ghost objekat za detekciju preklapanja sa igracem i nakon skupljanja nestaje dok ga sistem ponovo ne aktivira.

Klasa Physics omotava Bullet Physics svet i pruza metode za kreiranje i uklanjanje rigid body objekata, ghost objekata, raycast testiranje i detekciju preklapanja. Klasa InputHandler implementira Irrlicht interfejs IEventReceiver i prati stanje tastera na tastaturi i dugmadi misa, ukljucujuci mogucnost konzumiranja jednokratnih klikova.

Glavni petlja igre (game loop) izvrsava se unutar metode Game::run() i u svakoj iteraciji racuna proteklo vreme (delta time), azurira stanje igre u zavisnosti od trenutnog stanja automata, koraca fizicku simulaciju, sinhronizuje fizicke transformacije sa vizuelnim cvorovima i iscrtava scenu.


# 3. Mehanike igre

Survive koristi sistem talasa (wave system) za kontrolu tezine igre tokom trajanja partije od 180 sekundi. Tajmer odbrojava unazad od tri minuta i na osnovu preostalog vremena odredjuje trenutni talas. Prvi talas traje dok je tajmer iznad 120 sekundi, sa maksimalno tri ziva neprijatelja istovremeno i intervalom izmedju spawnovanja od pet sekundi. Drugi talas aktivira se kada tajmer padne ispod 120 sekundi i dozvoljava do sest neprijatelja sa intervalom od dve sekunde. Treci talas pocinje kada tajmer padne ispod 60 sekundi, dozvoljavajuci do deset neprijatelja sa intervalom od jedne sekunde. Kada tajmer dostigne nulu, igra prelazi u stanje pobede.

Neprijatelji se pojavljuju na jednoj od cetiri kapije koje su rasporedene na ivicama arene. Sistem nasumicno bira kapiju za svako novo pojavljivanje. Kada se neprijatelj stvori, on prvo hoda unapred kroz kapiju bez fizickog tela (faza SPAWNING) da bi vizuelno usao u arenu, nakon cega se kreira njegov fizicki oblik i on prelazi u fazu pozdrava (SALUTING) pre nego sto pocne da juri igraca.

Vestacka inteligencija neprijatelja zasniva se na direktnom pravolinijskom kretanju ka poziciji igraca. Neprijatelj racuna vektor od svoje pozicije do igraca, normalizuje ga i postavlja odgovarajucu brzinu na svom Bullet rigid body objektu. Brzina kretanja neprijatelja iznosi 160 jedinica u sekundi. Ukoliko neprijatelj detektuje da se zaglavio (presao manje od 5 jedinica za jednu sekundu), aktivira se mehanizam izbegavanja prepreka koji ga pomera bocno (strafe) u trajanju od 0.6 sekundi pre nego sto nastavi pravolinijsko kretanje.

Kao mehanizam balansiranja, neprijatelji povremeno zastaju i izvode animaciju pozdrava (salute) tokom jurenja. Ova pauza traje dve sekunde i ima cooldown izmedju tri i osam sekundi. Maksimalno tri zvuka pozdrava mogu se reprodukovati istovremeno kako bi se sprecilo zvucno preopterecenje.

Sistem napada funkcionise tako da samo jedan neprijatelj moze aktivno napadati igraca u datom trenutku. Ostali neprijatelji koji su dovoljno blizu (unutar 45 jedinica) prelaze u stanje cekanja (WAIT_ATTACK) gde stoje i gledaju ka igracu dok ne dobiju dozvolu za napad. Napad nanosi 10 poena stete sa cooldownom od jedne sekunde izmedju udaraca. Detekcija napada koristi ghost objekat u obliku sfere koji se postavlja ispred neprijatelja.

Igrac puca koriscenjem raycast mehanizma kroz Bullet Physics svet. Zrak se emituje sa pozicije grudi igraca u smeru u kom je okrenut, sa maksimalnim dometom od 300 jedinica. Svaki pogodak nanosi 25 poena stete neprijatelju. Sacmara ima fire rate od 0.8 sekundi izmedju pucnjeva. Ukoliko igrac nema municiju, umesto pucanja izvodi animaciju mahanja.

Zdravlje igraca pocinje na 100 poena. Kada primi stetu, igrac ulazi u kratku animaciju bola (0.4 sekunde) tokom koje ne moze da se krece niti puca. Ako zdravlje padne na nulu, igrac umire sa animacijom pada i igra prelazi u stanje GAMEOVER.

Na sest pozicija u areni rasporedeni su spawneri za municiju. Svi paketi municije startuju kao nevidljivi i pojavljuju se nasumicno, sa intervalom izmedju osam i dvadeset sekundi. Kada se pojavi, paket postaje vidljiv i moze se pokupiti dodirom, pri cemu igrac dobija sedam metaka. Nakon skupljanja, paket ponovo postaje nevidljiv i ceka sledecu aktivaciju od strane sistema.

Arena sadrzi razlicite prepreke u vidu stubova i kutija. Dvanaest stubova visokih izmedju 150 i 240 jedinica i dvanaest niskih kutija rasporedeno je po areni, svaki sa odgovarajucim fizickim telom za koliziju. Ove prepreke uticu na kretanje i igraca i neprijatelja.


# 4. Kontrole

Kretanje igraca vrsi se pomocu WASD tastera na tastaturi. Taster W pomera igraca napred u odnosu na smer kamere, S nazad, A levo i D desno. Igrac se automatski rotira u smeru kretanja. Brzina kretanja iznosi 200 jedinica u sekundi.

Kamera se kontrolise pomocu misa. Horizontalno pomeranje misa rotira kameru oko igraca sa osetljivoscu od 0.2 stepena po pikselu. Kamera se nalazi na rastojanju od 120 jedinica iza igraca i 30 jedinica iznad njega, uvek gledajuci ka igracu.

Levi klik misa aktivira pucanje. Igrac ispaljuje sacmaru u smeru u kom je okrenut. Ukoliko nema municije, umesto pucnja igrac izvodi animaciju mahanja rukom. Tokom animacije pucanja igrac ne moze da se krece.

Desni klik misa aktivira rezim nisanjenja. Dok je desno dugme misa pritisnuto, igrac se okrece u smeru kamere umesto u smeru kretanja, kretanje je zaustavljeno i pojavljuje se nisan na sredini ekrana. Ovaj rezim omogucava preciznije ciljanje neprijatelja.

Taster ESC tokom igre otvara meni za pauzu koji zaustavlja sve aktivnosti u igri i prikazuje kursor misa. Ponovni pritisak na ESC ili klik na dugme Resume nastavlja igru. Taster F1 ukljucuje i iskljucuje prikaz debug vizualizacije fizickih tela.

Tasteri 1 do 4 sluze za manuelno pojavljivanje neprijatelja na odgovarajucim kapijama, sto je korisno za testiranje tokom razvoja.


# 5. Korisnicki interfejs

Korisnicki interfejs igre sastoji se od glavnog menija, menija za pauzu, ekrana za kredite i HUD elemenata tokom igre.

Glavni meni prikazuje se pri pokretanju igre. Pozadinska slika prekriva ceo ekran, a tri dugmeta (Play, Credits i Exit) prikazuju se centrirana vertikalno na sredini ekrana. Dugmad su realizovana kao teksture koje se iscrtavaju pomocu Irrlicht-ove draw2DImage funkcije, a detekcija klika vrsi se proverom da li se pozicija kursora nalazi unutar pravougaonika dugmeta. Ovaj pristup koriscen je jer InputHandler vraca false iz OnEvent metode, sto znaci da standardni Irrlicht GUI elementi ne primaju dogadjaje.

Meni za pauzu aktivira se pritiskom na ESC tokom igre. Preko trenutne scene iscrtava se poluprovidan crni overlay koji zatamnjuje pozadinu, a na njemu se prikazuju dva dugmeta: Resume za nastavak igre i Exit za izlazak. Kursor misa postaje vidljiv tokom pauze.

Ekran za kredite prikazuje naziv igre na pozadini glavnog menija. Povratak na glavni meni vrsi se klikom ili pritiskom na ESC.

HUD (Heads-Up Display) tokom igre prikazuje nekoliko informacija. U gornjem levom delu ekrana nalazi se prikaz zdravlja igraca u formatu "HP: vrednost". U donjem desnom delu ekrana prikazuje se ikona metka i brojac preostale municije. Na vrhu ekrana, centralno, stoji tajmer u formatu minuta i sekundi koji odbrojava preostalo vreme do kraja partije. Ispod tajmera prikazuje se oznaka trenutnog talasa u zutoj boji. U gornjem desnom uglu prikazuje se brojac ubijenih neprijatelja.

Nisan (crosshair) prikazuje se na centru ekrana iskljucivo kada igrac drzi desni klik misa u rezimu nisanjenja. Svi HUD elementi automatski se sakrivaju kada igra nije u stanju PLAYING.


# 6. Fizika i kolizije

Fizicki svet simulira se pomocu Bullet Physics biblioteke koja se inicijalizuje u klasi Physics. Svet koristi btDiscreteDynamicsWorld sa standardnom konfiguracijom kolizija, dispatcherom, broadphase algoritmom (btDbvtBroadphase) i sekvencionim impulsnim solverom. Gravitacija sveta postavljena je na -9.81 po Y osi, mada je za igraca i neprijatelje gravitacija individualno iskljucena jer se kretanje kontrolise direktno postavljanjem brzine.

Igrac je fizicki predstavljen kao dinamicka kapsula (btCapsuleShape) sa radijusom od 15 i visinom od 30 jedinica i masom od 80. Rotacija kapsule je zakljucana (angular factor je nula) tako da se ne moze prevrnuti. Neprijatelji koriste istu vrstu kapsule sa masom od 10. Kada neprijatelj umre, njegovo fizicko telo se odmah uklanja iz sveta kako ne bi blokiralo kretanje ostalih objekata.

Pod arene simulira se kao statican box (masa nula) dimenzija 3000x1 jedinica po XZ ravni na visini od -25. Oko arene postavljeni su nevidljivi zidovi realizovani kao staticki box oblici koji sprecavaju igraca i neprijatelje da napuste igraliste. Neki od ovih zidova su rotirani da prate oblik kolosseuma.

Prepreke (stubovi i kutije) u areni imaju staticke box oblike koji se kreiraju pri pokretanju igre. Svaka prepreka ima vizuelni Irrlicht node i odgovarajuci Bullet rigid body sa nultom masom.

Za detekciju skupljanja pickupa i dometa napada neprijatelja koriste se ghost objekti (btGhostObject) koji ne generisu fizicki odgovor ali detektuju preklapanje sa drugim objektima. Pickup koristi sferni ghost objekat, a svaki neprijatelj ima sferni ghost objekat koji se pozicionira ispred njega i sluzi kao triger za napad.

Pucanje koristi Bullet-ov rayTest mehanizam. Zrak se emituje od pozicije grudi igraca u smeru u kom je okrenut, a ako pogodi rigid body koji pripada nekom neprijatelju, tom neprijatelju se nanosi steta.

Sinhronizacija izmedju fizickog i vizuelnog sveta vrsi se svaki frejm. Nakon koracanja fizicke simulacije, pozicija i rotacija svakog Bullet tela kopira se na odgovarajuci Irrlicht scene node pomocu metode syncPhysicsToNode iz bazne klase GameObject.


# 7. Audio sistem

Zvucni efekti u igri reprodukuju se pomocu IrrKlang biblioteke. Igrac i neprijatelji koriste zasebne instance zvucnog engine-a.

Za igraca postoji pet kategorija zvucnih efekata. Zvuk pucnja sacmare (shotgun.mp3) reprodukuje se pri svakom ispaljivanju sa jacinom od 0.5. Zvuk trcanja (running.mp3) pusta se u petlji dok se igrac krece i zaustavlja se kada stane, sa jacinom od 0.3. Kada igrac primi stetu, reprodukuje se zvuk bola (PAIN50_1.WAV) sa jacinom od 0.6. Zvuk smrti (death2.wav) pusta se jednom kada zdravlje padne na nulu, sa jacinom od 0.7. Pri skupljanju municije reprodukuje se zvuk pokupljenog predmeta (ammo_pickup.mp3) sa jacinom od 0.5.

Za neprijatelje postoji pet kategorija zvukova sa zasebno podesivim jacinama. Zvuk pozdrava (salute.mp3) pusta se kada neprijatelj udje u fazu pozdrava, sa jacinom od 0.3. Implementiran je sistem koji prati aktivne zvukove pozdrava i ne dozvoljava reprodukciju vise od tri istovremeno. Zvuk napada (attack.mp3) reprodukuje se pri svakom udarcu neprijatelja sa jacinom od 0.5. Zvuk bola (hurt.mp3) pusta se kada neprijatelj primi stetu ali ne umre, sa jacinom od 0.5. Zvuk smrti (death.mp3) reprodukuje se kada neprijatelj umre, sa jacinom od 0.6.

Poseban zvucni efekat je zvuk jurenja (chasing.mp3) koji se pusta u petlji dok god bar jedan neprijatelj aktivno juri igraca. Ovaj zvuk kontrolise klasa Game i reprodukuje se sa jacinom od 0.2. Kada nijedan neprijatelj ne juri igraca (svi su mrtvi, u fazi pozdrava ili u stanju mirovanja), zvuk se zaustavlja.


# 8. Screenshots

(Ovde ubaciti slike iz igre koje prikazuju glavni meni, gameplay tokom razlicitih talasa, pauza meni, HUD elemente, arenu sa preprekama i neprijatelje u razlicitim stanjima.)


# 9. Zakljucak

Tokom razvoja igre Survive primenjena su znanja iz oblasti racunarske grafike, fizickih simulacija i dizajna igara. Integracija tri nezavisne biblioteke (Irrlicht za renderovanje, Bullet Physics za kolizije i IrrKlang za zvuk) zahtevala je pazljivu sinhronizaciju izmedju vizuelnog i fizickog sveta u svakom frejmu, sto predstavlja jedan od kljucnih tehnickih izazova ovog projekta.

Arhitektura zasnovana na baznoj klasi GameObject omogucila je uniformno upravljanje svim objektima u sceni, dok je konacni automat za stanja igre i stanja neprijatelja obezbedio jasnu strukturu kontrole toka. Sistem talasa sa eskalacijom tezine, ogranicena municija i mehanika nisanjenja daju igri odredjenu takticku dimenziju jer igrac mora da rasporeduje resurse i bira trenutke za pucanje.

Jedan od interesantnijih aspekata implementacije je vestacka inteligencija neprijatelja koja kombinuje direktno kretanje ka igracu sa detekcijom zaglavljivanja i bocnim izbegavanjem prepreka. Mehanizam pozdrava kao pauze u jurenju sluzi kao balansiranje tezine jer sprecava da svi neprijatelji istovremeno napadaju igraca.

Projekat je realizovan u Visual Studio 2022 okruzenju za Windows platformu. Moguca unapredjenja ukljucuju implementaciju sistema za cuvanje rezultata, dodavanje razlicitih tipova neprijatelja sa razlicitim brzinama i zdravljem, prosirenje mape sa vise prostorija i implementaciju pathfinding algoritma umesto trenutnog pravolinijskog kretanja neprijatelja.
