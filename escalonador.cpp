#include <iostream>
#include <vector>
#include <queue>
#include <list>
#include <string>
#include <algorithm>
#include <iomanip>
#include <fstream>
#include <sstream>
#include <map>

using namespace std;

// ============================================================================
// ESTRUTURAS DE DADOS
// ============================================================================

struct Processo {
    int id;
    string nome;
    vector<int> paginas;
    int paginaAtual;
    int pageFaults;
    int tempoChegada;
    int tempoRetorno;
    int tempoBloqueado;
    int quantumConsumido;      // usado apenas para debug, mas não essencial agora
    bool finalizado;
    bool naFila;               // indica se já foi inserido na fila de prontos na chegada

    Processo(int _id, string _nome, vector<int> _paginas, int _tempoChegada = 0)
        : id(_id), nome(_nome), paginas(_paginas), paginaAtual(0), pageFaults(0),
          tempoChegada(_tempoChegada), tempoRetorno(0), tempoBloqueado(0),
          quantumConsumido(0), finalizado(false), naFila(false) {}
};

struct Frame {
    int pagina;
    int ultimoUso;
    int processoId;
    Frame() : pagina(-1), ultimoUso(-1) {}
};

// ============================================================================
// GERENCIADOR DE MEMÓRIA (LRU)
// ============================================================================

class GerenciadorMemoria {
private:
    vector<Frame> ram;
    int quantidadeFrames;

public:
    GerenciadorMemoria(int frames) : quantidadeFrames(frames) {
        ram.resize(frames);
    }

    bool paginaNaRam(int pagina)
    {
    for (const auto& frame : ram)
        if (frame.pagina == pagina)
            return true;

    return false;
    }

    void atualizarUso(int pagina, int tempoAtual)
    {
        for (auto& frame : ram)
        {
            if (frame.pagina == pagina)
            {
                frame.ultimoUso = tempoAtual;
                return;
            }
        }
    }
    int encontrarFrameLivre() {
        for (int i = 0; i < quantidadeFrames; i++)
            if (ram[i].pagina == -1) return i;
        return -1;
    }

    int encontrarPaginaLRU() {
        int indiceLRU = 0, menorTempo = ram[0].ultimoUso;
        for (int i = 1; i < quantidadeFrames; i++)
            if (ram[i].ultimoUso < menorTempo) {
                menorTempo = ram[i].ultimoUso;
                indiceLRU = i;
            }
        return indiceLRU;
    }

    // Retorna: {houve substituição, {página removida, processoId removido}}
    pair<bool,int> carregarPagina(int pagina, int tempoAtual)
    {
        int frameLivre = encontrarFrameLivre();

        if (frameLivre != -1)
        {
            ram[frameLivre].pagina = pagina;
            ram[frameLivre].ultimoUso = tempoAtual;

            return {false, -1};
        }

        int indiceLRU = encontrarPaginaLRU();
        int removida = ram[indiceLRU].pagina;

        ram[indiceLRU].pagina = pagina;
        ram[indiceLRU].ultimoUso = tempoAtual;

        return {true, removida};
    }

    string estadoRam(const map<int,string>& nomes) {
        string estado = "RAM: [";
        for (int i = 0; i < quantidadeFrames; i++) {
            if (ram[i].pagina == -1)
                estado += "Vazio";
            else {
                string nome = (nomes.count(ram[i].processoId)) ? nomes.at(ram[i].processoId)
                                                               : "P?";
                estado += "Pg" + to_string(ram[i].pagina);
            }
            if (i < quantidadeFrames - 1) estado += " | ";
        }
        return estado + "]";
    }

    int getQuantidadeFrames() { return quantidadeFrames; }
};

// ============================================================================
// ESCALONADOR ROUND ROBIN (agora gerencia também a CPU atual)
// ============================================================================

class EscalonadorRR {
private:
    queue<int> filaProntos;
    list<int>  filaBloqueados;
    int quantum;
    int processoAtual;      // -1 indica CPU ociosa
    int quantumRestante;    // ticks restantes para o processo atual

public:
    EscalonadorRR(int q) : quantum(q), processoAtual(-1), quantumRestante(0) {}

    void adicionarPronto(int processoId) {
        filaProntos.push(processoId);
    }

    // Tenta escalonar um novo processo, se a CPU estiver livre
    void escalonar() {
        if (processoAtual == -1 && !filaProntos.empty()) {
            processoAtual = filaProntos.front();
            filaProntos.pop();
            quantumRestante = quantum;
        }
    }

    // Deve ser chamado após um hit (execução normal)
    // Retorna true se o processo ainda deve continuar na CPU (quantum > 0 e não finalizado)
    bool tickExecutado(bool processoFinalizado) {
        if (processoFinalizado) {
            processoAtual = -1;
            quantumRestante = 0;
            return false;
        }
        quantumRestante--;
        if (quantumRestante == 0) {
            // preempção: volta para o final da fila
            if (processoAtual != -1) {
                filaProntos.push(processoAtual);
            }
            processoAtual = -1;
            return false;
        }
        return true; // continua na CPU no próximo tick
    }

    // Deve ser chamado em caso de page fault: processo perde a CPU e vai para bloqueados
    void bloquearAtual() {
        if (processoAtual != -1) {
            filaBloqueados.push_back(processoAtual);
            processoAtual = -1;
            quantumRestante = 0;
        }
    }

    // Retorna o processo que está atualmente na CPU, ou -1 se nenhum
    int getProcessoAtual() const {
        return processoAtual;
    }

    // Atualiza bloqueados: decrementa tempos e retorna os que desbloquearam
    vector<int> atualizarBloqueados(vector<Processo>& processos) {
        vector<int> desbloqueados;
        auto it = filaBloqueados.begin();
        while (it != filaBloqueados.end()) {
            int id = *it;
            processos[id].tempoBloqueado--;
            if (processos[id].tempoBloqueado <= 0) {
                desbloqueados.push_back(id);
                adicionarPronto(id);
                it = filaBloqueados.erase(it);
            } else ++it;
        }
        return desbloqueados;
    }

    bool temProcessos() const {
        return !filaProntos.empty() || !filaBloqueados.empty() || processoAtual != -1;
    }

    bool cpuOciosa() const { return processoAtual == -1; }
    int getQuantum() const { return quantum; }
    int tamanhoProntos() const { return (int)filaProntos.size(); }
    int tamanhoBloqueados() const { return (int)filaBloqueados.size(); }
};

// ============================================================================
// SIMULADOR PRINCIPAL
// ============================================================================

class Simulador {
private:
    vector<Processo>    processos;
    GerenciadorMemoria* memoria;
    EscalonadorRR*      escalonador;
    int                 tempoAtual;
    int                 penalidadeIO;
    int                 totalPageFaults;
    map<int,string>     nomesPorId;

    void log(const string& mensagem) {
        cout << "[Tempo " << setw(3) << tempoAtual << "] " << mensagem << "\n";
    }

    string nomeDoProcesso(int id) {
        return nomesPorId.count(id) ? nomesPorId[id] : "P?";
    }

public:
    Simulador(int quantum, int frames, int penalidade)
        : tempoAtual(0), penalidadeIO(penalidade), totalPageFaults(0) {
        memoria = new GerenciadorMemoria(frames);
        escalonador = new EscalonadorRR(quantum);
    }

    ~Simulador() { delete memoria; delete escalonador; }

    void adicionarProcesso(int id, string nome, vector<int> paginas, int tempoChegada) {
        processos.emplace_back(id, nome, paginas, tempoChegada);
        nomesPorId[id] = nome;
    }

    bool todosFinalizados() {
        for (const auto& p : processos)
            if (!p.finalizado) return false;
        return true;
    }

    void executar() {
        cout << "\n";
        cout << "           INICIO DA SIMULACAO DO SISTEMA OPERACIONAL 22           \n";
        cout << "  Quantum RR: " << setw(3) << escalonador->getQuantum()
             << "    Frames RAM: " << setw(3) << memoria->getQuantidadeFrames()
             << "    Penalidade I/O: " << setw(3) << penalidadeIO << " ticks        \n";
        cout << "\n========== LOG DE EXECUCAO ==========\n\n";

        int limiteSeguranca = 100000;

        while (!todosFinalizados() && limiteSeguranca-- > 0) {

            // 1. Chegada de novos processos
            for (auto& p : processos) {
                if (!p.finalizado && !p.naFila && p.tempoChegada == tempoAtual) {
                    p.naFila = true;
                    escalonador->adicionarPronto(p.id);
                    log("Processo " + p.nome + " chegou e foi adicionado a fila de prontos");
                }
            }

            // 2. Atualiza bloqueados e move desbloqueados para prontos
            vector<int> desbloqueados = escalonador->atualizarBloqueados(processos);
            for (int id : desbloqueados) {
                log("Processo " + processos[id].nome + " saiu da fila de bloqueados → fila de prontos");
            }

            // 3. Escalonamento: se CPU está ociosa, tenta pegar um processo da fila
            escalonador->escalonar();

            // 4. Se CPU continua ociosa, avança o tempo
            if (escalonador->cpuOciosa()) {
                if (escalonador->temProcessos())
                    log("CPU ociosa - aguardando processos");
                tempoAtual++;
                continue;
            }

            // 5. Obtém o processo atual
            int pid = escalonador->getProcessoAtual();
            Processo& p = processos[pid];
            if (p.finalizado) {
                // Segurança: não deveria acontecer, mas se sim, libera CPU
                escalonador->tickExecutado(true);
                tempoAtual++;
                continue;
            }

            int paginaDesejada = p.paginas[p.paginaAtual];

            // 6. Verifica se a página está na RAM
            if (memoria->paginaNaRam(paginaDesejada)) {
                // ===== RAM HIT =====
                memoria->atualizarUso(paginaDesejada, tempoAtual);

                log("Processo " + p.nome + " executou pagina " +
                    to_string(paginaDesejada) + " (HIT)");
                cout << "           " << memoria->estadoRam(nomesPorId) << "\n";

                p.paginaAtual++;
                bool finalizado = (p.paginaAtual >= (int)p.paginas.size());

                if (finalizado) {
                    p.finalizado = true;
                    p.tempoRetorno = (tempoAtual + 1) - p.tempoChegada;
                    log(">>> Processo " + p.nome + " FINALIZADO (tempo de retorno: " +
                        to_string(p.tempoRetorno) + " ticks) <<<");
                }

                // Atualiza quantum e possível preempção
                bool continua = escalonador->tickExecutado(finalizado);
                if (!continua && !finalizado) {
                    log("Processo " + p.nome +
                        " sofreu preempcao (quantum esgotado) → fim da fila de prontos");
                }
                // Se continua, o mesmo processo continua na CPU no próximo tick

            } else {
                // ===== PAGE FAULT =====
                p.pageFaults++;
                totalPageFaults++;

                log("Processo " + p.nome + " sofreu Page Fault na pagina " +
                    to_string(paginaDesejada));

                auto resultado = memoria->carregarPagina(paginaDesejada, tempoAtual);

                if (resultado.first) {
                    log("LRU: pagina " + to_string(resultado.second) +
                        " removida da RAM");
                }

                log("Pagina " + to_string(paginaDesejada) +
                    " (" + p.nome + ") carregada na RAM");
                cout << "           " << memoria->estadoRam(nomesPorId) << "\n";

                // Processo perde a CPU e vai para bloqueados
                p.tempoBloqueado = penalidadeIO;
                escalonador->bloquearAtual();
                log("Processo " + p.nome + " movido para fila de bloqueados (" +
                    to_string(penalidadeIO) + " ticks)");
                // Nota: paginaAtual não avança, pois a falha ocorreu antes de executar a página
            }

            tempoAtual++;
        }

        if (limiteSeguranca <= 0)
            cerr << "\nAVISO: limite de segurança atingido (possivel loop infinito).\n";

        imprimirRelatorio();
    }

    void imprimirRelatorio() {
        cout << "\n";
        cout << "                        RELATORIO FINAL                           \n";
        cout << "\n\n";

        for (const auto& p : processos) {
            cout << "\n";
            cout << " Processo " << p.nome << "\n";
            cout << "\n";
            cout << "  Tempo de chegada : " << setw(5) << p.tempoChegada << " ticks\n";
            cout << "  Tempo de retorno : " << setw(5) << p.tempoRetorno << " ticks\n";
            cout << "  Total Page Faults: " << setw(5) << p.pageFaults << "\n";
            cout << "  Paginas          : ";
            for (int pg : p.paginas) cout << pg << " ";
            cout << "\n\n\n";
        }

        cout << "\n";
        cout << "          ESTATISTICAS GLOBAIS            \n";
        cout << "  Tempo total da simulacao: " << setw(5) << tempoAtual << " ticks  \n";
        cout << "  Total de Page Faults:     " << setw(5) << totalPageFaults << "        \n";
        cout << "\n";
    }
};

// ============================================================================
// UTILITÁRIOS
// ============================================================================

struct InfoProcesso {
    int tempoChegada;
    string nome;
    vector<int> paginas;
    InfoProcesso(int t, const string& n, const vector<int>& p)
        : tempoChegada(t), nome(n), paginas(p) {}
    bool operator<(const InfoProcesso& outro) const {
        return tempoChegada < outro.tempoChegada;
    }
};

vector<int> parsearPaginas(const string& str) {
    vector<int> paginas;
    stringstream ss(str);
    string token;
    while (getline(ss, token, ','))
        if (!token.empty()) paginas.push_back(stoi(token));
    return paginas;
}

bool carregarArquivo(const string& nomeArquivo, Simulador*& simulador,
                     vector<InfoProcesso>& processosInfo) {
    ifstream arquivo(nomeArquivo);
    if (!arquivo.is_open()) {
        cerr << "Erro: nao foi possivel abrir '" << nomeArquivo << "'\n";
        return false;
    }

    int quantum, frames, penalidade;
    if (!(arquivo >> quantum >> frames >> penalidade)) {
        cerr << "Erro: formato invalido na primeira linha\n";
        return false;
    }

    cout << "\nParametros carregados:\n";
    cout << "  Quantum      : " << quantum << "\n";
    cout << "  Frames RAM   : " << frames << "\n";
    cout << "  Penalidade IO: " << penalidade << " ticks\n";

    simulador = new Simulador(quantum, frames, penalidade);

    int tempoChegada; string nome, paginasStr;
    cout << "\nProcessos carregados:\n";
    while (arquivo >> tempoChegada >> nome >> paginasStr) {
        vector<int> paginas = parsearPaginas(paginasStr);
        cout << "  " << nome << " (chegada t=" << tempoChegada << ") paginas: ";
        for (size_t i = 0; i < paginas.size(); i++) {
            cout << paginas[i];
            if (i + 1 < paginas.size()) cout << ",";
        }
        cout << "\n";
        processosInfo.emplace_back(tempoChegada, nome, paginas);
    }
    arquivo.close();
    return true;
}

int main(int argc, char* argv[]) {
    cout << "    SIMULADOR DE SISTEMAS OPERACIONAIS - Round Robin + LRU      \n";
    cout << "         Universidade do Vale do Itajai - Trabalho M2            \n";

    Simulador* simulador = nullptr;
    vector<InfoProcesso> processosInfo;

    if (argc >= 2) {
        cout << "\n[Modo Arquivo] Carregando: " << argv[1] << "\n";
        if (!carregarArquivo(argv[1], simulador, processosInfo)) return 1;
        sort(processosInfo.begin(), processosInfo.end());
        for (size_t i = 0; i < processosInfo.size(); i++)
            simulador->adicionarProcesso((int)i, processosInfo[i].nome,
                                         processosInfo[i].paginas,
                                         processosInfo[i].tempoChegada);
    } else {
        cout << "\n[Modo Interativo] (use ./simulador config.txt para carregar arquivo)\n\n";

        int quantum, frames, penalidade, numProcessos;
        cout << "Quantum Round Robin     : "; cin >> quantum;
        cout << "Frames de RAM           : "; cin >> frames;
        cout << "Penalidade I/O (ticks)  : "; cin >> penalidade;
        cout << "Numero de processos     : "; cin >> numProcessos;

        simulador = new Simulador(quantum, frames, penalidade);
        cin.ignore();
        cout << "\nDigite as paginas de cada processo separadas por virgula (ex: 1,2,5)\n\n";

        for (int i = 0; i < numProcessos; i++) {
            string nome = "P" + to_string(i + 1);
            int chegada; string paginasStr;
            cout << "Tempo de chegada de " << nome << " : "; cin >> chegada;
            cin.ignore();
            cout << "Paginas de " << nome << "          : "; getline(cin, paginasStr);
            vector<int> paginas = parsearPaginas(paginasStr);
            if (paginas.empty()) {
                cout << "AVISO: " << nome << " sem paginas, ignorado.\n";
                continue;
            }
            simulador->adicionarProcesso(i, nome, paginas, chegada);
        }
    }

    simulador->executar();
    cout << "\n========== FIM DA SIMULACAO ==========\n\n";
    delete simulador;
    return 0;
}
