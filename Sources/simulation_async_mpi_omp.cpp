#include <cstdlib>
#include <random>
#include <iostream>
#include <fstream>
#include "contexte.hpp"
#include "individu.hpp"
#include "graphisme/src/SDL2/sdl2.hpp"
#include <mpi.h>
#include <chrono>

std::ofstream output("Courbe.dat");

void màjStatistique( épidémie::Grille& grille, std::vector<épidémie::Individu> const& individus )
{
     # pragma omp parallel for schedule(dynamic)
    for ( auto& statistique : grille.getStatistiques() )
    {
        statistique.nombre_contaminant_grippé_et_contaminé_par_agent = 0;
        statistique.nombre_contaminant_seulement_contaminé_par_agent = 0;
        statistique.nombre_contaminant_seulement_grippé              = 0;
    }
    auto [largeur,hauteur] = grille.dimension();
    auto& statistiques = grille.getStatistiques();
     # pragma omp parallel for schedule(dynamic)
    for ( auto const& personne : individus )
    {
        auto pos = personne.position();

        std::size_t index = pos.x + pos.y * largeur;
        if (personne.aGrippeContagieuse() )
        {
            if (personne.aAgentPathogèneContagieux())
            {
                statistiques[index].nombre_contaminant_grippé_et_contaminé_par_agent += 1;
            }
            else 
            {
                statistiques[index].nombre_contaminant_seulement_grippé += 1;
            }
        }
        else
        {
            if (personne.aAgentPathogèneContagieux())
            {
                statistiques[index].nombre_contaminant_seulement_contaminé_par_agent += 1;
            }
        }
    }
}


void majGrille( épidémie::Grille& grille, std::vector<int> V1, std::vector<int> V2, std::vector<int> V3)
{
    # pragma omp parallel for 
    for ( auto& statistique : grille.getStatistiques() )
    {
        statistique.nombre_contaminant_grippé_et_contaminé_par_agent = 0;
        statistique.nombre_contaminant_seulement_contaminé_par_agent = 0;
        statistique.nombre_contaminant_seulement_grippé              = 0;
    }
    
    auto& statistiques = grille.getStatistiques();
    std::size_t S = V1.size();
    # pragma omp parallel for
    for ( std::size_t index=0; index<S; index++)
    {
        
        statistiques[index].nombre_contaminant_seulement_grippé = V1[index];
        statistiques[index].nombre_contaminant_seulement_contaminé_par_agent = V2[index];
        statistiques[index].nombre_contaminant_grippé_et_contaminé_par_agent = V3[index];
        
    }
}

void afficheSimulation(sdl2::window& écran,std::vector<int> valuesGrippe, std::vector<int> valuesAgent, std::size_t jour,int largeur_grille,int hauteur_grille)
{
    auto [largeur_écran,hauteur_écran] = écran.dimensions();
    sdl2::font fonte_texte("./graphisme/src/data/Lato-Thin.ttf", 18);
    écran.cls({0x00,0x00,0x00});
    // Affichage de la grille :
    std::uint16_t stepX = largeur_écran/largeur_grille;
    unsigned short stepY = (hauteur_écran-50)/hauteur_grille;
    double factor = 255./15.;

    for ( unsigned short i = 0; i < largeur_grille; ++i )
    {
        for (unsigned short j = 0; j < hauteur_grille; ++j )
        {
            int valueGrippe = valuesGrippe[i+j*largeur_grille];
            int valueAgent = valuesAgent[i+j*largeur_grille];
            std::uint16_t origx = i*stepX;
            std::uint16_t origy = j*stepY;
            std::uint8_t red = valueGrippe > 0 ? 127+std::uint8_t(std::min(128., 0.5*factor*valueGrippe)) : 0;
            std::uint8_t green = std::uint8_t(std::min(255., factor*valueAgent));
            std::uint8_t blue= std::uint8_t(std::min(255., factor*valueAgent ));
            écran << sdl2::rectangle({origx,origy}, {stepX,stepY}, {red, green,blue}, true);
        }
    }

    écran << sdl2::texte("Carte population grippée", fonte_texte, écran, {0xFF,0xFF,0xFF,0xFF}).at(largeur_écran/2, hauteur_écran-20);
    écran << sdl2::texte(std::string("Jour : ") + std::to_string(jour), fonte_texte, écran, {0xFF,0xFF,0xFF,0xFF}).at(0,hauteur_écran-20);
    écran << sdl2::flush;
}

void simulation(bool affiche)
{
    MPI_Comm globComm;
    MPI_Comm_dup(MPI_COMM_WORLD, &globComm);
    MPI_Comm Comm_affichage, Comm_sim;
    int nbp;
    MPI_Comm_size(globComm, &nbp);
    int rank;
    MPI_Status status;
    MPI_Comm_rank(globComm, &rank);


    if (rank==0) MPI_Comm_split(MPI_COMM_WORLD, 0, rank, &Comm_affichage);
    else MPI_Comm_split(MPI_COMM_WORLD, 1, rank, &Comm_sim);
    
    std::uniform_real_distribution<double> porteur_pathogène(0.,1.);


    unsigned int graine_aléatoire = 1;


        épidémie::ContexteGlobal contexte;
        // contexte.déplacement_maximal = 1; <= Si on veut moins de brassage
        // contexte.taux_population = 400'000;
        //contexte.taux_population = 1'000;
        contexte.interactions.β = 60.;
        std::vector<épidémie::Individu> population;
        population.reserve(contexte.taux_population);
        épidémie::Grille grille{contexte.taux_population};

        auto [largeur_grille,hauteur_grille] = grille.dimension();
        // L'agent pathogène n'évolue pas et reste donc constant...
        épidémie::AgentPathogène agent(graine_aléatoire++);
        // Initialisation de la population initiale :

    épidémie::Grippe grippe(0);
    

    if (rank==1) {
        
        output << "# jours_écoulés \t nombreTotalContaminésGrippe \t nombreTotalContaminésAgentPathogène()" << std::endl;
    }

    if (rank!=0)
    {
        std::vector<épidémie::Individu> sub_population;
        std::size_t sub_taux = (contexte.taux_population)/(nbp-1);
        sub_population.reserve(sub_taux);
    
        graine_aléatoire=graine_aléatoire+(rank-1)*sub_taux;
        
        for (std::size_t i = sub_taux*(rank-1); i < sub_taux * rank; ++i )
        {
            std::default_random_engine motor(100*(i+1));
            sub_population.emplace_back(graine_aléatoire++, contexte.espérance_de_vie, contexte.déplacement_maximal);
            sub_population.back().setPosition(largeur_grille, hauteur_grille);
            if (porteur_pathogène(motor) < 0.2)
            {
                sub_population.back().estContaminé(agent);  
            }
        }

        std::size_t jours_écoulés = 0;
        int         jour_apparition_grippe = 0;
        int         nombre_immunisés_grippe = (contexte.taux_population*23)/100;
        

        
        

        


        std::cout << "Début boucle épidémie" << std::endl << std::flush;
        while (1)
        {
            auto start = std::chrono::system_clock::now();
            
            if (jours_écoulés%365 == 0)// Si le premier Octobre (début de l'année pour l'épidémie ;-) )
            {
                grippe = épidémie::Grippe(jours_écoulés/365);
                jour_apparition_grippe = grippe.dateCalculImportationGrippe();
                grippe.calculNouveauTauxTransmission();
                // 23% des gens sont immunisés. On prend les 23% premiers
                for ( int ipersonne = 0; ipersonne < (int)sub_taux; ++ipersonne)
                {
                    if(ipersonne+(int)sub_taux*(rank-1)<nombre_immunisés_grippe){
                    sub_population[ipersonne].devientImmuniséGrippe();}
                    else {
                    sub_population[ipersonne].redevientSensibleGrippe();}
                }
            }
            if (jours_écoulés%365 == std::size_t(jour_apparition_grippe))
            {
                for (int ipersonne = 0; ipersonne < (int)sub_taux; ++ipersonne )
                {
                    if (ipersonne+(rank-1)*(int)sub_taux<nombre_immunisés_grippe + 25){
                    population[ipersonne].estContaminé(grippe);
                }
            }
            }
            // Mise à jour des statistiques pour les cases de la grille :
            màjStatistique(grille, population);
            // On parcout la population pour voir qui est contaminé et qui ne l'est pas, d'abord pour la grippe puis pour l'agent pathogène
            std::size_t compteur_grippe = 0, compteur_agent = 0, mouru = 0;
            # pragma omp parallel for reduction(+:compteur_grippe, compteur_agent, mouru)
            for ( auto& personne : population )
            {
                if (personne.testContaminationGrippe(grille, contexte.interactions, grippe, agent))
                {
                    compteur_grippe ++;
                    personne.estContaminé(grippe);
                }
                if (personne.testContaminationAgent(grille, agent))
                {
                    compteur_agent ++;
                    personne.estContaminé(agent);
                }
                // On vérifie si il n'y a pas de personne qui veillissent de veillesse et on génère une nouvelle personne si c'est le cas.
                if (personne.doitMourir())
                {
                    mouru++;
                    unsigned nouvelle_graine = jours_écoulés + personne.position().x*personne.position().y;
                    personne = épidémie::Individu(nouvelle_graine, contexte.espérance_de_vie, contexte.déplacement_maximal);
                    personne.setPosition(largeur_grille, hauteur_grille);
                }
                personne.veillirDUnJour();
                personne.seDéplace(grille);
            }



            // valeurs à partager avec les autres process 
            
            std::vector<épidémie::Grille::StatistiqueParCase> V = grille.getStatistiques();
            int s1, s2, s3;
            std::vector<int> V1, V2, V3;
            for (auto& Case : V){
                int v1 = Case.nombre_contaminant_seulement_grippé;
                MPI_Allreduce(&v1, &s1, 1, MPI_INT, MPI_SUM, Comm_sim);
                V1.push_back(s1);
                int v2 = Case.nombre_contaminant_seulement_contaminé_par_agent;
                MPI_Allreduce(&v2, &s2, 1, MPI_INT, MPI_SUM, Comm_sim);
                V2.push_back(s2);
                int v3 = Case.nombre_contaminant_grippé_et_contaminé_par_agent;
                MPI_Allreduce(&v3, &s3, 1, MPI_INT, MPI_SUM, Comm_sim);
                V3.push_back(s3);
                
            }
            
            
            majGrille(grille,V1,V2,V3);
            //#############################################################################################################
            //##    Affichage des résultats pour le temps  actuel
            //#############################################################################################################

            if (rank ==1){
            int flag = 0;
            MPI_Iprobe( 0, 10, MPI_COMM_WORLD, &flag, &status );
            if (flag)
            {

            int ready;
            MPI_Recv(&ready,1,MPI_INT,0,10,globComm,&status);
            //envoyer les donnes d'affichage au procesus d'affichage

            auto const& statistiques = grille.getStatistiques();
            int sz = statistiques.size();
            std::vector<int> nbr_contaminant_seulement_grippé;
            std::vector<int> nbr_contaminant_seulement_contaminé_par_agent;
            std::vector<int> nbr_contaminant_grippé_et_contaminé_par_agent;
            for (auto statistique : statistiques)
            {
                nbr_contaminant_seulement_grippé.push_back(statistique.nombre_contaminant_seulement_grippé);
                nbr_contaminant_seulement_contaminé_par_agent.push_back(statistique.nombre_contaminant_seulement_contaminé_par_agent);
                nbr_contaminant_grippé_et_contaminé_par_agent.push_back(statistique.nombre_contaminant_grippé_et_contaminé_par_agent);
            }
            MPI_Send(&sz,1,MPI_INT,0,7,globComm);
            MPI_Send(nbr_contaminant_seulement_grippé.data(),sz,MPI_INT,0,4,globComm);
            MPI_Send(nbr_contaminant_seulement_contaminé_par_agent.data(),sz,MPI_INT,0,5,globComm);
            MPI_Send(nbr_contaminant_grippé_et_contaminé_par_agent.data(),sz,MPI_INT,0,6,globComm);
            MPI_Send(&jours_écoulés,1,MPI_INT,0,8,globComm);
            /*std::cout << jours_écoulés << "\t" << grille.nombreTotalContaminésGrippe() << "\t"
                  << grille.nombreTotalContaminésAgentPathogène() << std::endl;*/
            }
            output << jours_écoulés << "\t" << grille.nombreTotalContaminésGrippe() << "\t"<< grille.nombreTotalContaminésAgentPathogène() << std::endl;
            jours_écoulés += 1;
            auto end = std::chrono::system_clock::now();
            auto elapsed = std::chrono::duration_cast<std::chrono::milliseconds>(end - start);
            std::cout << elapsed.count() << '\n';
            }
        }
        // Fin boucle temporelle
        output.close();
    }

    if (rank==0)
    {
        constexpr const unsigned int largeur_écran = 1280, hauteur_écran = 1024;
        sdl2::window écran("Simulation épidémie de grippe", {largeur_écran,hauteur_écran});
        while(1)
        {

            //envoie un message pour notifier l'autre processus qu'on est près à recevoir
            int ready = 1;
            MPI_Send(&ready,1,MPI_INT,1,10,globComm);
            //reception des donnees
            int sz;
            MPI_Recv(&sz,1,MPI_INT,1,7,globComm,&status);
            int jours_écoulés;
    

            std::vector<int> nbr_contaminant_seulement_grippé(sz);
            std::vector<int> nbr_contaminant_seulement_contaminé_par_agent(sz);
            std::vector<int> nbr_contaminant_grippé_et_contaminé_par_agent(sz);
            MPI_Recv(nbr_contaminant_seulement_grippé.data(),sz,MPI_INT,1,4,globComm,&status);
            MPI_Recv(nbr_contaminant_seulement_contaminé_par_agent.data(),sz,MPI_INT,1,5,globComm,&status);
            MPI_Recv(nbr_contaminant_grippé_et_contaminé_par_agent.data(),sz,MPI_INT,1,6,globComm,&status);
            MPI_Recv(&jours_écoulés,1,MPI_INT,1,8,globComm,&status);
        
            std::vector<int> valuesGrippe;
            std::vector<int> valuesAgent;

            for ( unsigned short i = 0; i < largeur_grille; ++i )
            {
                
                for (unsigned short j = 0; j < hauteur_grille; ++j )
                {
                    auto stat = i+j*largeur_grille;
                    int valueGrippe = nbr_contaminant_grippé_et_contaminé_par_agent[stat]+nbr_contaminant_seulement_grippé[stat];
                    int valueAgent  = nbr_contaminant_grippé_et_contaminé_par_agent[stat]+nbr_contaminant_seulement_contaminé_par_agent[stat];
                    //if (valueGrippe !=0) std::cout<<valueGrippe<< std::endl;
                    valuesGrippe.push_back(valueGrippe);
                    valuesAgent.push_back(valueAgent);
                    //~ std::cout<<valuesAgent[k] << std::endl;
                    //~ k++;
                }
            }



            if (affiche) afficheSimulation(écran, valuesGrippe, valuesAgent, jours_écoulés, largeur_grille,hauteur_grille);
            

        }
    }
}

int main(int argc, char* argv[])
{
    // parse command-line
    MPI_Init( &argc, &argv );

    bool affiche = true;
    for (int i=0; i<argc; i++) {
      std::cout << i << " " << argv[i] << "\n";
      if (std::string("-nw") == argv[i]) affiche = false;
    }
  
    sdl2::init(argc, argv);
    {
        simulation(affiche);
    }
    sdl2::finalize();

    MPI_Finalize();

    return EXIT_SUCCESS;
}
