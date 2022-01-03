### Pour les questions à 2 processeurs j'ai utlisé mon mac :
	machdep.cpu.brand_string: Intel(R) Core(TM) i5-5257U CPU @ 2.70GHz
	machdep.cpu.core_count: 2
	machdep.cpu.cores_per_package: 8
	machdep.cpu.extfamily: 0
	machdep.cpu.extfeature_bits: 1241984796928
	machdep.cpu.extfeatures: SYSCALL XD 1GBPAGE EM64T LAHF LZCNT PREFETCHW RDTSCP TSCI
	machdep.cpu.extmodel: 3
	machdep.cpu.family: 6


2.1 : Mesure de temps.
	 temps passé dans la simulation par pas de temps avec aﬀichage : 75 ms
	 temps passé dans la simulation par pas de temps hors aﬀichage : 22 ms
	 temps passé à l’aﬀichage : 45 ms

	le temps passé à l'affichage est deux fois plus grand du temps passé à la simulation seule.


2.2 : Parallélisation aﬀichage contre simulation.
		
	 J'interviens sur la boucle d'affichage et simulation. Je la divise en deux boucles, une de simulation et une autre pour l'affichage. chacune sur un processeur. Lorsque la boucle de simulation fait son calcul elle envoie le contenu de statistiques de la grille à celle de l'affichage. J'ai modifié la fonction d'affichage pour utlisé le vecteur statistiques au lieu de la grille.

	 temps passé par pas de temps pour la simulation : 46 ms. Par rapport à la question précedente le speed-up est de 1.63. On est limité par la vitesse d'affichage (~45ms) car il est synchrone. On calcule un pas de temps dans la simulation mais on attend que l'affiche se termine pour continuer.

2.3 : Parallélisation aﬀichage asynchrone contre simulation.
	 
	 Pour rendre l'affichage asynchrone, quand la boucle d'affichage est prête pour une mise à jour elle envoie un message à celle de simulation. avec Iprobe cette dernière vérifie si ce message est prêt à être reçu. si oui elle envoie les nouveaux données d'affichage avant de continuer la simulation, sinon elle continue son travail.

	 temps passé par pas de temps pour la simulation : 21 ms (speed up de 2.14 par rapport à la question précedente), on est au temps de simulation hors affichage. on attend pas la fin d'affichage pour continuer. cela va dans le même sens que l’analyse faite dans la partie précédente 
(Connaissance acquise : MPI_Iprobe)

2.4 : Parallélisation OpenMP
	
	j'ai intervenu sur la boucle for de la fonction majStatistique, elle ne nécessite pad de protection de variables.
	pour la boucle en simulation je l'ai parralelisée avec reduction+ pour compteur_grippe, compteur_agent et mouru.
	J'ai essayé de parrleliser la boucle de generation de la poulation initiale en protégant la garine avec atomic, cela a compilé mais m'a donné des erreurs mystérieuses dans la compilation (je l'ai laissée en séquentiel puisu'elle est parcourue une seule fois).




	un nombre d’individus global constant
	 1 thread : 1
	 2 thread : 1.35
	 3 thread : 1.6
	 4 thread : 1.8
	 5 thread : 1.39
	 6 thread : 1.48
	 8 thread : 1.61
	On constate une baisse des performances après 4 threads puis une remontée. Ceci est dû au passage en hyperthreading. 

	un nombre d’individus constant (400000) par thread

	 1 thread : 1
	 2 thread : 0.66
	 3 thread : 0.52
	 4 thread : 0.43

	 J'ai pas une très bonne scalabilité.


2.5 : Parallélisation MPI de la simulation

	Le processeur 0 s'occupe de l'affichage. Chacun des autres autres s'occupe d'une partie de la population.
	J'utilise reduce pour que les processeurs partagent les vecteurs de statistiques, et j'ai ajouté une fonction qui met à jour la grille à partir des vecteurs. (Compile mais j'obtiens pas des résultats correctes)
(Connaissance acquise : MPI_Split)


2.6 : Parallélisation finale

	En se basant sur la version Parallélisation MPI (qui a encore besoin des corrections), je fais de même pour  la Parallélisation OpenMP, J'ai parrallélisé les boucles for des fonctions de mise à jour, et les boucles de simulation.


