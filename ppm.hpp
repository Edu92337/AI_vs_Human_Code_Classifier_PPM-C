#pragma once
#include<iostream>
#include<array>
#include<set>
#include<queue>
#include"codificador_aritmetico.hpp"
#include"estrutura_contexto.hpp"

using namespace std;
typedef struct Simbolo Simbolo;
struct Simbolo{
    uint8_t byte;
    uint64_t bits_emitidos; // sequência de bits emitida pelo codificador
};


struct Ppm{
    trie_contexto arvore;
    deque<uint8_t> janela_atual;
    Codificador_aritmetico aritmetico;
    set<uint8_t>excluidos; // bytes ja vistos -> precisa ser limpo a cada novo byte 
    No* equiprovaveis;
    // Kmax e J vão ser passados como parâmetros da compilação
    int Kmax;
    int J;
    int adapta;
    uint64_t bits_inicial;
    uint64_t bits_final;
    uint64_t comprimento_emitido; // para análise de métricas
    uint64_t total_simbolos_processados;
    double l0 = 0,lf = 0;
    long long bytes_na_janela = 0;
    bool treino;
    // ---- Contadores e log de eventos de adaptação (poda/reset) ----
    Ppm(int k,bool treinando){
        Kmax = k;
        equiprovaveis = new No();
        inicia_equiprovaveis();
        total_simbolos_processados = 0;
        treino = treinando;
    }

    ~Ppm(){
        delete equiprovaveis;
    }

    void inicia_equiprovaveis(){
        for(int i = 0;i<256;i++) equiprovaveis->freq_ref((uint16_t)i) = 1;
        equiprovaveis->total = 256;
    }

    void reinicia_modelo(){
        aritmetico.low  = 0;
        aritmetico.high = (uint32_t)Codificador_aritmetico::TOP;
        aritmetico.bits_pendentes = 0;
        inicia_equiprovaveis();
        janela_atual.clear(); // Limpa a janela antes de começar a codificar o novo arquivo
    }

    bool existe_contexto(No* contexto,uint8_t simbolo){
        return contexto->get_freq(simbolo) > 0;
    }
    No* busca_maior_contexto(deque<uint8_t>&janela){
        // Busca o maior contexto possivel para aquela janela de bytes e tenta codificar o simbolo
        No* maior_contexto_possivel = arvore.busca_contexto_byte(janela);
        return maior_contexto_possivel;
    }
    void atualiza_frequencia_contexto(No* contexto,uint8_t atual){
        // atualiza no contexto usado
        arvore.atualiza_frequencia(contexto,atual);
    }

    uint32_t calcula_escape(No* contexto){
        return max(1u, static_cast<uint32_t>(contexto->distintos));
    }
    void insere_em_excluidos(No* contexto){
        for(const auto& [s,f] : contexto->frequencias){
            if(f > 0 && s < 256) excluidos.insert((uint8_t)s);
        }
    }
    
    void atualiza_janela(uint8_t atual){
        janela_atual.push_back(atual);
        if (janela_atual.size() > (size_t)Kmax)
            janela_atual.pop_front();
    }

    bool atualiza_contexto(uint8_t atual){
        atualiza_janela(atual);
        if (treino)
            return arvore.insere_byte_em_contexto(janela_atual);
        return true;
    }

    void processa_simbolo(uint8_t atual){
        total_simbolos_processados++;
        bool codificado = false;
        bits_inicial = aritmetico.bits_emitidos_total;
        No* contexto = arvore.busca_contexto_byte(janela_atual);
        No* contexto_inicial = contexto;
        // percorre, subindo, procurando onde codificar o simbolo
        // a raiz está inclusa
        while(contexto) {
            if(excluidos.count(atual) == 0) {
                // Injeta ESCAPE temporariamente para que decode_byte() 
                //veja a mesma distribuição de probabilidade que encode_byte()
                // estava dando erro de sincronia, o decode não conseguia 
                // ver a mesma distribuição
                uint32_t freq_esc = calcula_escape(contexto);
                contexto->freq_ref(ESCAPE) = freq_esc;
                contexto->total += freq_esc;

                if(existe_contexto(contexto, atual)) {
                    // Símbolo existe: codifica ele usando a tabela que INCLUI o ESCAPE
                    aritmetico.encode_byte(atual, contexto, excluidos);
                    codificado = true;

                    // Remove a injeção temporária antes de terminar
                    contexto->freq_ref(ESCAPE) = 0;
                    contexto->total -= freq_esc;
                    break; // Termina o loop, encontramos o símbolo
                } 
                else {
                    // Símbolo não existe: codifica o ESCAPE
                    aritmetico.encode_byte(ESCAPE, contexto, excluidos);

                    // Remove a injeção temporária
                    contexto->freq_ref(ESCAPE) = 0;
                    contexto->total -= freq_esc;

                    insere_em_excluidos(contexto);
                }
            }
            // Se o símbolo já foi excluído por um nível superior, sobe silenciosamente
            contexto = contexto->pai;
        }
        

        if(codificado == false){
            // Se não codificou em nenhum contexto, codifica com equiprováveis
            aritmetico.encode_byte(atual, equiprovaveis, excluidos);
        }

        // atualiza o tamanho emitido para o símbolo atual
        bits_final = aritmetico.bits_emitidos_total; 
        bytes_na_janela++;

        // sempre propaga a partir do contexto de ORDEM MAIS ALTA
        // tentado (contexto_inicial), independente de qual nível efetivamente
        // codificou o símbolo. Isso garante que todos os contextos no caminho
        // -1,0,1,...,Kmax aprendam sobre "atual", inclusive os que deram ESCAPE.

        if(treino){   // só atualiza os contextos/frequências durante o treino
            arvore.atualiza_frequencia(contexto_inicial, atual);
        }
        atualiza_contexto(atual);

        // limpar excluidos para processar um byte novo
        excluidos.clear();

    }


};