#include <iostream>
#include <vector>
#include <fstream>
#include <filesystem>
#include <system_error>
#include <cstdlib>
#include <algorithm>
#include <cmath>
#include "codificador_aritmetico.hpp"
#include "estrutura_contexto.hpp"
#include "ppm.hpp"

using namespace std;
namespace fs = filesystem;

const string BASE = "/home/eduardo/Faculdade/Introdução a Teoria da Informação/Projeto-2/dataset_resplit";
// Monta o caminho para qualquer subconjunto do dataset:
string caminho_dataset(const string& conjunto, const string& classe, const string& tipo){
    return BASE + "/" + conjunto + "/" + classe + "/" + tipo;
}

// Para pastas SEM subpasta por linguagem (ex: dataset_validacao/validacao_ia,
// onde os arquivos de todas as linguagens ficam juntos, diferenciados só
// pelo prefixo do nome: "c__", "cpp__", "java__", "py__").
string caminho_dataset_flat(const string& conjunto, const string& classe){
    return BASE + "/" + conjunto + "/" + classe;
}


void diagnostico_scores(const vector<pair<double,bool>>& scores_rotulos,
                        const string& rotulo_contexto)
{
    long long n_ia = 0, n_humano = 0;
    double soma_ia = 0.0, soma_humano = 0.0;

    for(const auto& [score, eh_ia] : scores_rotulos){
        if(eh_ia){
            n_ia++;
            soma_ia += score;
        }else{
            n_humano++;
            soma_humano += score;
        }
    }

    double media_ia     = (n_ia > 0) ? soma_ia / n_ia : 0.0;
    double media_humano = (n_humano > 0) ? soma_humano / n_humano : 0.0;

    double var_ia = 0.0;
    double var_humano = 0.0;

    for(const auto& [score, eh_ia] : scores_rotulos){
        if(eh_ia)
            var_ia += (score - media_ia) * (score - media_ia);
        else
            var_humano += (score - media_humano) * (score - media_humano);
    }

    double desvio_ia =
        (n_ia > 1) ? sqrt(var_ia / (n_ia - 1)) : 0.0;

    double desvio_humano =
        (n_humano > 1) ? sqrt(var_humano / (n_humano - 1)) : 0.0;

    cout << "[DIAGNOSTICO " << rotulo_contexto << "] "
         << "IA: n=" << n_ia
         << " media=" << media_ia
         << " desvio=" << desvio_ia
         << " | Humano: n=" << n_humano
         << " media=" << media_humano
         << " desvio=" << desvio_humano
         << endl;
}

void codifica_arquivo(ifstream& arquivo, Ppm& modelo){
    modelo.reinicia_modelo();
    char byte;
    while(arquivo.get(byte)){
        modelo.processa_simbolo((uint8_t)byte);
    }
    modelo.aritmetico.finaliza_codificacao();
    
}
// Calcula o comprimento médio do arquivo em bits por símbolo, usando o modelo fornecido.
double comprimento_do_arquivo(ifstream& arquivo, Ppm& modelo){
    uint64_t bits_antes = modelo.aritmetico.bits_emitidos_total;
    uint64_t simbolos_antes = modelo.total_simbolos_processados;

    codifica_arquivo(arquivo, modelo);
    uint64_t n = modelo.total_simbolos_processados - simbolos_antes;
    if(n == 0) return 0.0;
    uint64_t bits_depois = modelo.aritmetico.bits_emitidos_total;
    return (double)(bits_depois - bits_antes) / (double)n;
}

void treina_modelo(Ppm& modelo,bool ia, string tipo){
    error_code ec;
    string path = caminho_dataset("dataset_treino_efetivo", ia ? "ai_treino" : "human_treino", tipo);
    
    for(auto& entrada : fs::recursive_directory_iterator(path, fs::directory_options::skip_permission_denied, ec)){
        if(ec){ cerr << "Erro: " << ec.message() << endl; return; }

        if(fs::is_regular_file(entrada.status())){
            ifstream arquivo(entrada.path(), ios::binary);
            if(!arquivo.is_open()){
                cerr << "Erro ao abrir: " << entrada.path() << endl;
                return;
            }
            codifica_arquivo(arquivo, modelo);
        }
    }
}

string classificador(Ppm& modelo_ia, Ppm& modelo_humano, string path_arquivo, bool teste_unitario, double threshold){
    ifstream arquivo(path_arquivo, ios::binary);
    if(!arquivo.is_open()){
        cerr << "Erro ao abrir: " << path_arquivo << endl;
        return "";
    }

    double comprimento_ia = comprimento_do_arquivo(arquivo, modelo_ia);
    arquivo.clear();
    arquivo.seekg(0);
    double comprimento_humano = comprimento_do_arquivo(arquivo, modelo_humano);

    double score = comprimento_ia - comprimento_humano;

    if(teste_unitario){
        cout << "Arquivo: " << path_arquivo << endl;
        cout << "Comprimento IA: " << comprimento_ia << endl;
        cout << "Comprimento Humano: " << comprimento_humano << endl;
        cout << "Score (ia - humano): " << score << endl;
        cout << "Threshold usado: " << threshold << endl;
    }

    if (score < threshold) return "IA";
    else return "Humano";
}


void teste_geral(Ppm& modelo_ia, Ppm& modelo_humano, string& path_ia, string& path_humano, double threshold){
    string path;
    long long corretos_humano = 0, total_humano = 0;
    long long corretos_ia = 0, total_ia = 0;
    vector<pair<double,bool>> scores; // para diagnóstico, mesmo formato usado na validação

    path = path_humano;
    error_code ec;
    for(auto& entrada : fs::recursive_directory_iterator(path, fs::directory_options::skip_permission_denied, ec)){
        if(ec){ cerr << "Erro: " << ec.message() << endl; return; }
        if(!fs::is_regular_file(entrada.status())) continue;

        ifstream arquivo(entrada.path(), ios::binary);
        if(!arquivo.is_open()) continue;
        double comp_ia = comprimento_do_arquivo(arquivo, modelo_ia);
        arquivo.clear();
        arquivo.seekg(0);
        double comp_humano = comprimento_do_arquivo(arquivo, modelo_humano);
        double score = comp_ia - comp_humano;
        scores.push_back({score, false});

        if(score >= threshold) corretos_humano++;
        total_humano++;
    }

    path = path_ia;
    for(auto& entrada : fs::recursive_directory_iterator(path, fs::directory_options::skip_permission_denied, ec)){
        if(ec){ cerr << "Erro: " << ec.message() << endl; return; }
        if(!fs::is_regular_file(entrada.status())) continue;

        ifstream arquivo(entrada.path(), ios::binary);
        if(!arquivo.is_open()) continue;
        double comp_ia = comprimento_do_arquivo(arquivo, modelo_ia);
        arquivo.clear();
        arquivo.seekg(0);
        double comp_humano = comprimento_do_arquivo(arquivo, modelo_humano);
        double score = comp_ia - comp_humano;
        scores.push_back({score, true});

        if(score < threshold) corretos_ia++;
        total_ia++;
    }

    diagnostico_scores(scores, "TESTE");
    double acuracia_humano = 100.0 * corretos_humano / total_humano;
    double acuracia_ia = 100.0 * corretos_ia / total_ia;
    double acuracia_total = 100.0 * (corretos_humano + corretos_ia) / (total_humano + total_ia);

    cout << "Threshold usado: " << threshold << endl;
    cout << "Humano:    " << corretos_humano << "/" << total_humano << " (" << acuracia_humano << "%)" << endl;
    cout << "IA:        " << corretos_ia << "/" << total_ia << " (" << acuracia_ia << "%)" << endl;
    cout << "Agregada:   " << acuracia_total << "% " << endl;
}
void teste_geral_tipo(Ppm& modelo_ia, Ppm& modelo_humano, string tipo, double threshold){
    string path_ia = caminho_dataset("dataset", "ai_teste", tipo);
    string path_humano = caminho_dataset("dataset", "human_teste", tipo);
    teste_geral(modelo_ia, modelo_humano, path_ia, path_humano, threshold);
}

void teste_arquivo(Ppm& modelo_ia, Ppm& modelo_humano, string& path_teste, double threshold){
    string classe = classificador(modelo_ia,modelo_humano,path_teste,true,threshold);
    cout << "Arquivo: " << path_teste << " -> Classe: " << classe << endl;
}


void exporta_csv_comprimentos(Ppm& modelo_ia, Ppm& modelo_humano,
                               string& path_ia, string& path_humano,
                               const string& tipo)
{
    string nome_csv = "comprimentos_" + tipo + ".csv";
    ofstream csv(nome_csv);
    if(!csv.is_open()){
        cerr << "Erro ao criar CSV: " << nome_csv << endl;
        return;
    }
    csv << "arquivo,tipo,tamanho_bytes,comprimento_ia,comprimento_humano,rotulo_real\n";

    error_code ec;

    // Arquivos Humanos
    for(auto& entrada : fs::recursive_directory_iterator(path_humano, fs::directory_options::skip_permission_denied, ec)){
        uintmax_t tamanho = fs::file_size(entrada.path(), ec);
        if(ec){
            tamanho = 0;
            ec.clear();
        }
        if(!fs::is_regular_file(entrada.status())) continue;

        ifstream arquivo(entrada.path(), ios::binary);
        if(!arquivo.is_open()){
            cerr << "Erro ao abrir: " << entrada.path() << endl;
            continue;
        }

        double comp_ia = comprimento_do_arquivo(arquivo, modelo_ia);
        arquivo.clear();
        arquivo.seekg(0);
        double comp_humano = comprimento_do_arquivo(arquivo, modelo_humano);

        csv << "\"" << entrada.path().string() << "\","
            << tipo << ","
            << tamanho << ","
            << comp_ia << ","
            << comp_humano << ","
            << "Humano\n";
    }

    // Arquivos IA
    for(auto& entrada : fs::recursive_directory_iterator(path_ia, fs::directory_options::skip_permission_denied, ec)){
        uintmax_t tamanho = fs::file_size(entrada.path(), ec);
        if(ec){
            tamanho = 0;
            ec.clear();
        }
        if(!fs::is_regular_file(entrada.status())) continue;

        ifstream arquivo(entrada.path(), ios::binary);
        if(!arquivo.is_open()){
            cerr << "Erro ao abrir: " << entrada.path() << endl;
            continue;
        }

        double comp_ia = comprimento_do_arquivo(arquivo, modelo_ia);
        arquivo.clear();
        arquivo.seekg(0);
        double comp_humano = comprimento_do_arquivo(arquivo, modelo_humano);

        csv << "\"" << entrada.path().string() << "\","
            << tipo << ","
            << tamanho << ","
            << comp_ia << ","
            << comp_humano << ","
            << "IA\n";
    }

    csv.close();
    cout << "[INFO] CSV exportado: " << nome_csv << endl;
}

void exporta_csv_comprimentos_tipo(Ppm& modelo_ia, Ppm& modelo_humano, const string& tipo){
    string path_ia = caminho_dataset("dataset", "ai_teste", tipo);
    string path_humano = caminho_dataset("dataset", "human_teste", tipo);
    exporta_csv_comprimentos(modelo_ia, modelo_humano, path_ia, path_humano, tipo);
}

// Coleta os scores (comprimento_ia - comprimento_humano) de todos os arquivos em uma pasta plana (sem subpastas),
// filtrando pelo prefixo do nome do arquivo, se fornecido.
void coleta_scores_flat(Ppm& modelo_ia, Ppm& modelo_humano, const string& path,
                        bool rotulo_eh_ia, vector<pair<double,bool>>& saida,
                        const string& prefixo_filtro = ""){
    
    error_code ec;
    for(auto& entrada : fs::directory_iterator(path, fs::directory_options::skip_permission_denied, ec)){
        if(ec){ cerr << "Erro: " << ec.message() << endl; return; }
        if(!fs::is_regular_file(entrada.status())) continue;

        // Filtra pelo prefixo do nome do arquivo (ex: "py__") quando a pasta
        // mistura arquivos de várias linguagens.
        if(!prefixo_filtro.empty()){
            string nome_arquivo = entrada.path().filename().string();
            if(nome_arquivo.rfind(prefixo_filtro, 0) != 0) continue; // não começa com o prefixo
        }

        ifstream arquivo(entrada.path(), ios::binary);
        if(!arquivo.is_open()) continue;

        double comp_ia = comprimento_do_arquivo(arquivo, modelo_ia);
        arquivo.clear();
        arquivo.seekg(0);
        double comp_humano = comprimento_do_arquivo(arquivo, modelo_humano);

        double score = comp_ia - comp_humano; // menor => mais "parece IA"
        saida.push_back({score, rotulo_eh_ia});
    }
}

// Calcula o threshold ótimo por Youden's J a partir dos scores coletados.
// Convenção: classifica IA se score < threshold.
double calcula_threshold_youden(vector<pair<double,bool>>& scores_rotulos){
    // Ordena os scores e calcula TPR, FPR e J para cada ponto de corte possível.
    // Retorna o threshold que maximiza J = TPR - FPR.
    if(scores_rotulos.empty()) return 0.0;

    sort(scores_rotulos.begin(), scores_rotulos.end());

    long long total_ia = 0, total_humano = 0;
    for(auto& [s, eh_ia] : scores_rotulos) eh_ia ? total_ia++ : total_humano++;

    if(total_ia == 0 || total_humano == 0){
        cerr << "[AVISO] Uma das classes não tem amostras suficientes para calibrar threshold." << endl;
        return 0.0;
    }

    double melhor_threshold = scores_rotulos.front().first;
    double melhor_j = -1.0;

    long long acumulado_ia = 0, acumulado_humano = 0;

    for(size_t i = 0; i < scores_rotulos.size(); i++){
        auto [s, eh_ia] = scores_rotulos[i];
        eh_ia ? acumulado_ia++ : acumulado_humano++;

        bool proximo_diferente = (i + 1 == scores_rotulos.size()) || (scores_rotulos[i+1].first != s);
        
        if(!proximo_diferente) continue;

        double tpr = (double)acumulado_ia / total_ia;
        double fpr = (double)acumulado_humano / total_humano;
        double j = tpr - fpr;

        if(j > melhor_j){
            // Atualiza o melhor threshold para o ponto médio entre este score e o próximo score diferente
            melhor_j = j;
            // Se este é o último score, usamos um valor maior que ele para o próximo score
            double proximo_score = (i + 1 < scores_rotulos.size()) ? scores_rotulos[i+1].first : s + 1.0;
            melhor_threshold = (s + proximo_score) / 2.0;
        }
    }

    cout << "[INFO] Threshold calibrado: " << melhor_threshold
         << "  (Youden J = " << melhor_j << ")" << endl;
    return melhor_threshold;
}

// Calibra o threshold usando dataset_validacao (arquivos nunca vistos no treino).
double calibra_threshold_validacao(Ppm& modelo_ia, Ppm& modelo_humano,
                                    const string& path_validacao_ia,
                                    const string& path_validacao_humano,
                                    const string& prefixo_filtro = "")
{
    vector<pair<double,bool>> scores;
    coleta_scores_flat(modelo_ia, modelo_humano, path_validacao_ia, true, scores, prefixo_filtro);
    coleta_scores_flat(modelo_ia, modelo_humano, path_validacao_humano, false, scores, prefixo_filtro);
    diagnostico_scores(scores, "VALIDACAO");
    return calcula_threshold_youden(scores);
}

// Calibra o threshold usando SOMENTE os arquivos de validação da linguagem
// (tipo) que está sendo testada — as pastas validacao_ia/validacao_human
// não têm subpasta por linguagem, então filtramos pelo prefixo do nome
// do arquivo (ex: "py__", "cpp__").
double calibra_threshold_validacao_tipo(Ppm& modelo_ia, Ppm& modelo_humano, const string& tipo)
{
    string path_validacao_ia     = caminho_dataset_flat("dataset_validacao", "validacao_ia");
    string path_validacao_humano = caminho_dataset_flat("dataset_validacao", "validacao_human");
    string prefixo = tipo + "__";
    return calibra_threshold_validacao(modelo_ia, modelo_humano, path_validacao_ia, path_validacao_humano, prefixo);
}