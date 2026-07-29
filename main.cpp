#include <iostream>
#include <vector>
#include <fstream>
#include <filesystem>
#include <system_error>
#include <cstdlib>
#include <cstring>
#include "codificador_aritmetico.hpp"
#include "estrutura_contexto.hpp"
#include "ppm.hpp"
#include "utils.hpp"
using namespace std;
namespace fs = filesystem;

int main(int argc, char* argv[]){
    int k;

    // Verifica se a flag --csv foi passada em qualquer posição
    bool modo_csv = false;
    for(int i = 1; i < argc; i++){
        if(strcmp(argv[i], "--csv") == 0){
            modo_csv = true;
            break;
        }
    }

    if(argc < 3){
        cerr << "Uso:" << endl;
        cerr << "  Testar um arquivo especifico: " << argv[0] << " <Kmax> <path_arquivo> <tipo>" << endl;
        cerr << "  Testar o dataset inteiro:     " << argv[0] << " <Kmax> <tipo> [--csv]" << endl;
        return 1;
    }

    if(modo_csv){
        // Modo exportação CSV: ./a <Kmax> <tipo> --csv
        k = atoi(argv[1]);
        string tipo = argv[2];

        Ppm modelo_ia(k, true);
        Ppm modelo_humano(k, true);

        treina_modelo(modelo_ia, true, tipo);
        treina_modelo(modelo_humano, false, tipo);

        modelo_ia.treino = false;
        modelo_humano.treino = false;

        double threshold = calibra_threshold_validacao_tipo(modelo_ia, modelo_humano, tipo);
        cout << "[INFO] Threshold (usar no plot_metricas.py --threshold): "<< threshold << endl;
        exporta_csv_comprimentos_tipo(modelo_ia, modelo_humano, tipo);
        return 0;
    }

    if(argc == 4){
        // Modo teste de arquivo específico: ./a <Kmax> <path_arquivo> <tipo>
        k = atoi(argv[1]);
        string path_arquivo = argv[2];
        string tipo = argv[3];
        Ppm modelo_ia(k,true);
        Ppm modelo_humano(k,true);
        treina_modelo(modelo_ia,true,tipo);
        treina_modelo(modelo_humano,false,tipo);
        modelo_ia.treino = false;
        modelo_humano.treino = false;
        double threshold = calibra_threshold_validacao_tipo(modelo_ia, modelo_humano, tipo);

        teste_arquivo(modelo_ia,modelo_humano,path_arquivo,threshold);
    }else {
        // Modo teste do dataset inteiro: ./a <Kmax> <tipo>
        k = atoi(argv[1]);
        string tipo = argv[2];
        Ppm modelo_ia(k,true);
        Ppm modelo_humano(k,true);

        treina_modelo(modelo_ia,true,tipo);
        treina_modelo(modelo_humano,false,tipo);

        modelo_ia.treino = false;
        modelo_humano.treino = false;
        double threshold = calibra_threshold_validacao_tipo(modelo_ia, modelo_humano, tipo);

        teste_geral_tipo(modelo_ia,modelo_humano,tipo,threshold);
    }

    return 0;
}