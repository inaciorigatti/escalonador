#include <iostream>
#include <vector>
#include <queue>
#include <list>
#include <string>
#include <algorithm>
#include <iomanip>
#include <fstream>
#include <sstream>

using namespace std;

// ============================================================================
// ESTRUTURAS DE DADOS
// ============================================================================

/**
 * Estrutura que representa um Processo no sistema
 */
struct Processo {
    int id;                     // Identificador único do processo
    string nome;                // Nome do processo (ex: "P1")
    vector<int> paginas;        // Sequência de páginas que o processo deseja acessar
    int paginaAtual;            // Índice da próxima página a ser acessada
    int pageFaults;             // Quantidade de falhas de página sofridas
    int tempoChegada;           // Instante em que o processo chegou no sistema
    int tempoRetorno;           // Tempo de retorno = conclusão - chegada
    int tempoBloqueado;         // Contador de ticks restantes no bloqueio
    int quantumConsumido;       // Quantidade de ticks usados no quantum atual
    bool finalizado;            // Indica se o processo terminou todas as páginas
    bool naFila;                // Indica se já foi adicionado à fila de prontos

    Processo(int _id, string _nome, vector<int> _paginas, int _tempoChegada = 0)
        : id(_id), nome(_nome), paginas(_paginas), paginaAtual(0), pageFaults(0),
        tempoChegada(_tempoChegada), tempoRetorno(0), tempoBloqueado(0),
        quantumConsumido(0), finalizado(false), naFila(false) {}
};

/**
 * Estrutura que representa um Frame na memória RAM
 */
struct Frame {
    int pagina;     // Número da página armazenada (-1 se vazio)
    int ultimoUso;  // Timestamp do último uso (para LRU)
    int processoId; // ID do processo dono da página

    Frame() : pagina(-1), ultimoUso(-1), processoId(-1) {}
};

// ============================================================================
// CLASSE GERENCIADOR DE MEMÓRIA (LRU)
// ============================================================================

class GerenciadorMemoria {
private:
    vector<Frame> ram;
    int quantidadeFrames;

public:
    GerenciadorMemoria(int frames) : quantidadeFrames(frames) {
        ram.resize(frames);
    }

    bool paginaNaRam(int pagina, int processoId) {
        for (const auto& frame : ram) {
            if (frame.pagina == pagina && frame.processoId == processoId) {
                return true;
            }
        }
        return false;
    }

    void atualizarUso(int pagina, int processoId, int tempoAtual) {
        for (auto& frame : ram) {
            if (frame.pagina == pagina && frame.processoId == processoId) {
                frame.ultimoUso = tempoAtual;
                return;
            }
        }
    }

    int encontrarFrameLivre() {
        for (int i = 0; i < quantidadeFrames; i++) {
            if (ram[i].pagina == -1) return i;
        }
        return -1;
    }

    int encontrarPaginaLRU() {
        int indiceLRU = 0;
        int menorTempo = ram[0].ultimoUso;
        for (int i = 1; i < quantidadeFrames; i++) {
            if (ram[i].ultimoUso < menorTempo) {
                menorTempo = ram[i].ultimoUso;
                indiceLRU = i;
            }
        }
        return indiceLRU;
    }

    // Retorna: {houve substituição, {página removida, processo removido}}
    pair<bool, pair<int, int>> carregarPagina(int pagina, int processoId, int tempoAtual) {
        int frameLivre = encontrarFrameLivre();
        if (frameLivre != -1) {
            ram[frameLivre].pagina = pagina;
            ram[frameLivre].processoId = processoId;
            ram[frameLivre].ultimoUso = tempoAtual;
            return { false, {-1, -1} };
        }
        else {
            int indiceLRU = encontrarPaginaLRU();
            int paginaRemovida = ram[indiceLRU].pagina;
            int processoRemovido = ram[indiceLRU].processoId;
            ram[indiceLRU].pagina = pagina;
            ram[indiceLRU].processoId = processoId;
            ram[indiceLRU].ultimoUso = tempoAtual;
            return { true, {paginaRemovida, processoRemovido} };
        }
    }

    string estadoRam() {
        string estado = "RAM: [";
        for (int i = 0; i < quantidadeFrames; i++) {
            if (ram[i].pagina == -1) {
                estado += "Vazio";
            }
            else {
                estado += "P" + to_string(ram[i].processoId) + ":Pg" + to_string(ram[i].pagina);
            }
            if (i < quantidadeFrames - 1) estado += " | ";
        }
        estado += "]";
        return estado;
    }

    int getQuantidadeFrames() { return quantidadeFrames; }
};

// ============================================================================
// CLASSE ESCALONADOR ROUND ROBIN
// ============================================================================

class EscalonadorRR {
private:
    queue<int> filaProntos;
    list<int>  filaBloqueados;
    int quantum;

public:
    EscalonadorRR(int q) : quantum(q) {}

    void adicionarPronto(int processoId) {
        filaProntos.push(processoId);
    }

    int proximoProcesso() {
        if (filaProntos.empty()) return -1;
        int id = filaProntos.front();
        filaProntos.pop();
        return id;
    }

    void bloquearProcesso(int processoId) {
        filaBloqueados.push_back(processoId);
    }

    // Decrementa tempoBloqueado e move para prontos quem zerou.
    // Retorna lista dos IDs desbloqueados.
    vector<int> atualizarBloqueados(vector<Processo>& processos) {
        vector<int> desbloqueados;
        auto it = filaBloqueados.begin();
        while (it != filaBloqueados.end()) {
            int id = *it;
            processos[id].tempoBloqueado--;
            if (processos[id].tempoBloqueado <= 0) {
                desbloqueados.push_back(id);
                filaProntos.push(id);
                it = filaBloqueados.erase(it);
            }
            else {
                ++it;
            }
        }
        return desbloqueados;
    }

    bool temProcessos() { return !filaProntos.empty() || !filaBloqueados.empty(); }
    bool filaProntosVazia() { return filaProntos.empty(); }
    bool filaBloqueadosVazia() { return filaBloqueados.empty(); }
    int  getQuantum() { return quantum; }
    int  tamanhoProntos() { return (int)filaProntos.size(); }
    int  tamanhoBloqueados() { return (int)filaBloqueados.size(); }
};

// ============================================================================
// CLASSE PRINCIPAL DO SIMULADOR
// ============================================================================

class Simulador {
private:
    vector<Processo>   processos;
    GerenciadorMemoria* memoria;
    EscalonadorRR* escalonador;
    int tempoAtual;
    int penalidadeIO;
    int totalPageFaults;

    void log(const string& mensagem) {
        cout << "[Tempo " << setw(3) << tempoAtual << "] " << mensagem << "\n";
    }

public:
    Simulador(int quantum, int frames, int penalidade)
        : tempoAtual(0), penalidadeIO(penalidade), totalPageFaults(0) {
        memoria = new GerenciadorMemoria(frames);
        escalonador = new EscalonadorRR(quantum);
    }

    ~Simulador() {
        delete memoria;
        delete escalonador;
    }

    void adicionarProcesso(int id, string nome, vector<int> paginas, int tempoChegada) {
        processos.emplace_back(id, nome, paginas, tempoChegada);
        // NÃO adiciona à fila aqui; será adicionado no tick correto
    }

    bool todosFinalizados() {
        for (const auto& p : processos)
            if (!p.finalizado) return false;
        return true;
    }

    void executar() {
        cout << "\n";
        cout << "╔══════════════════════════════════════════════════════════════════╗\n";
        cout << "║            INÍCIO DA SIMULAÇÃO DO SISTEMA OPERACIONAL            ║\n";
        cout << "╠══════════════════════════════════════════════════════════════════╣\n";
        cout << "║  Quantum RR: " << setw(3) << escalonador->getQuantum()
            << "  |  Frames RAM: " << setw(3) << memoria->getQuantidadeFrames()
            << "  |  Penalidade I/O: " << setw(3) << penalidadeIO << " ticks        ║\n";
        cout << "╚══════════════════════════════════════════════════════════════════╝\n";
        cout << "\n========== LOG DE EXECUÇÃO ==========\n\n";

        // Limite de segurança para evitar loop infinito em caso de bug
        int limiteSeguranca = 100000;

        while (!todosFinalizados() && limiteSeguranca-- > 0) {

            // 1. Verifica chegada de novos processos neste tick
            for (auto& p : processos) {
                if (!p.finalizado && !p.naFila && p.tempoChegada == tempoAtual) {
                    p.naFila = true;
                    escalonador->adicionarPronto(p.id);
                    log("Processo " + p.nome + " chegou e foi adicionado à fila de prontos");
                }
            }

            // 2. Atualiza processos bloqueados (decrementa e libera quem zerou)
            vector<int> desbloqueados = escalonador->atualizarBloqueados(processos);
            for (int id : desbloqueados) {
                log("Processo " + processos[id].nome + " saiu da fila de bloqueados → fila de prontos");
            }

            // 3. CPU ociosa?
            if (escalonador->filaProntosVazia()) {
                if (!escalonador->filaBloqueadosVazia() || !todosFinalizados()) {
                    log("CPU ociosa - aguardando processos");
                }
                tempoAtual++;
                continue;
            }

            // 4. Escala o próximo processo
            int pid = escalonador->proximoProcesso();
            Processo& p = processos[pid];

            if (p.finalizado) { tempoAtual++; continue; }

            int paginaDesejada = p.paginas[p.paginaAtual];

            if (memoria->paginaNaRam(paginaDesejada, p.id)) {
                // ===== RAM HIT =====
                memoria->atualizarUso(paginaDesejada, p.id, tempoAtual);

                log("Processo " + p.nome + " executou página " +
                    to_string(paginaDesejada) + " (HIT)");
                cout << "           " << memoria->estadoRam() << "\n";

                p.quantumConsumido++;
                p.paginaAtual++;

                if (p.paginaAtual >= (int)p.paginas.size()) {
                    // Processo terminou todas as páginas
                    p.finalizado = true;
                    // Tempo de retorno = instante de conclusão - instante de chegada
                    p.tempoRetorno = (tempoAtual + 1) - p.tempoChegada;
                    log(">>> Processo " + p.nome + " FINALIZADO (tempo de retorno: " +
                        to_string(p.tempoRetorno) + " ticks) <<<");

                }
                else if (p.quantumConsumido >= escalonador->getQuantum()) {
                    // Quantum esgotado → preempção
                    log("Processo " + p.nome + " sofreu preempção (quantum esgotado) → fim da fila de prontos");
                    p.quantumConsumido = 0;
                    escalonador->adicionarPronto(p.id);

                }
                else {
                    // Ainda tem quantum restante → volta ao topo da fila imediatamente
                    escalonador->adicionarPronto(p.id);
                }

            }
            else {
                // ===== PAGE FAULT =====
                p.pageFaults++;
                totalPageFaults++;

                log("Processo " + p.nome + " sofreu Page Fault na página " +
                    to_string(paginaDesejada));

                auto resultado = memoria->carregarPagina(paginaDesejada, p.id, tempoAtual);

                if (resultado.first) {
                    int pgRemovida = resultado.second.first;
                    int procRemovido = resultado.second.second;
                    log("LRU: página " + to_string(pgRemovida) +
                        " (P" + to_string(procRemovido) + ") removida da RAM");
                }

                log("Página " + to_string(paginaDesejada) +
                    " (" + p.nome + ") carregada na RAM");
                cout << "           " << memoria->estadoRam() << "\n";

                // Reset do quantum ao ser interrompido por page fault
                p.quantumConsumido = 0;
                // paginaAtual NÃO avança — o processo tentará a mesma página ao voltar

                p.tempoBloqueado = penalidadeIO;
                escalonador->bloquearProcesso(p.id);

                log("Processo " + p.nome + " movido para fila de bloqueados (" +
                    to_string(penalidadeIO) + " ticks)");
            }

            tempoAtual++;
        }

        if (limiteSeguranca <= 0)
            cerr << "\nAVISO: limite de segurança atingido (possível loop infinito).\n";

        imprimirRelatorio();
    }

    void imprimirRelatorio() {
        cout << "\n";
        cout << "╔══════════════════════════════════════════════════════════════════╗\n";
        cout << "║                        RELATÓRIO FINAL                           ║\n";
        cout << "╚══════════════════════════════════════════════════════════════════╝\n\n";

        for (const auto& p : processos) {
            cout << "┌──────────────────────────────────────────┐\n";
            cout << "│ Processo " << p.nome << "\n";
            cout << "├──────────────────────────────────────────┤\n";
            cout << "│  Tempo de chegada : " << setw(5) << p.tempoChegada << " ticks\n";
            cout << "│  Tempo de retorno : " << setw(5) << p.tempoRetorno << " ticks\n";
            cout << "│  Total Page Faults: " << setw(5) << p.pageFaults << "\n";
            cout << "│  Páginas          : ";
            for (int pg : p.paginas) cout << pg << " ";
            cout << "\n└──────────────────────────────────────────┘\n\n";
        }

        cout << "╔══════════════════════════════════════════╗\n";
        cout << "║          ESTATÍSTICAS GLOBAIS            ║\n";
        cout << "╠══════════════════════════════════════════╣\n";
        cout << "║  Tempo total da simulação: " << setw(5) << tempoAtual << " ticks  ║\n";
        cout << "║  Total de Page Faults:     " << setw(5) << totalPageFaults << "        ║\n";
        cout << "╚══════════════════════════════════════════╝\n";
    }
};

// ============================================================================
// UTILITÁRIOS
// ============================================================================

/**
 * Struct auxiliar para armazenar informações de processo lidas do arquivo.
 * Substitui std::tuple para garantir compatibilidade com MSVC.
 */
struct InfoProcesso {
    int tempoChegada;
    string nome;
    vector<int> paginas;

    InfoProcesso(int t, const string& n, const vector<int>& p)
        : tempoChegada(t), nome(n), paginas(p) {}

    // Necessário para sort() por tempoChegada
    bool operator<(const InfoProcesso& outro) const {
        return tempoChegada < outro.tempoChegada;
    }
};

vector<int> parsearPaginas(const string& str) {
    vector<int> paginas;
    stringstream ss(str);
    string token;
    while (getline(ss, token, ',')) {
        if (!token.empty()) paginas.push_back(stoi(token));
    }
    return paginas;
}

/**
 * Formato do arquivo de configuração:
 *   Linha 1 : quantum frames penalidade
 *   Demais  : tempoChegada NomeProcesso paginas  (ex: 0 P1 1,2,5)
 */
bool carregarArquivo(const string& nomeArquivo, Simulador*& simulador,
    vector<InfoProcesso>& processosInfo) {
    ifstream arquivo(nomeArquivo);
    if (!arquivo.is_open()) {
        cerr << "Erro: não foi possível abrir '" << nomeArquivo << "'\n";
        return false;
    }

    int quantum, frames, penalidade;
    if (!(arquivo >> quantum >> frames >> penalidade)) {
        cerr << "Erro: formato inválido na primeira linha\n";
        return false;
    }

    cout << "\nParâmetros carregados:\n";
    cout << "  Quantum      : " << quantum << "\n";
    cout << "  Frames RAM   : " << frames << "\n";
    cout << "  Penalidade IO: " << penalidade << " ticks\n";

    simulador = new Simulador(quantum, frames, penalidade);

    int tempoChegada;
    string nome, paginasStr;
    cout << "\nProcessos carregados:\n";
    while (arquivo >> tempoChegada >> nome >> paginasStr) {
        vector<int> paginas = parsearPaginas(paginasStr);
        cout << "  " << nome << " (chegada t=" << tempoChegada << ") páginas: ";
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

// ============================================================================
// MAIN
// ============================================================================

int main(int argc, char* argv[]) {
    cout << "\n";
    cout << "╔══════════════════════════════════════════════════════════════════╗\n";
    cout << "║     SIMULADOR DE SISTEMAS OPERACIONAIS - Round Robin + LRU      ║\n";
    cout << "║         Universidade do Vale do Itajaí - Trabalho M2            ║\n";
    cout << "╚══════════════════════════════════════════════════════════════════╝\n";

    Simulador* simulador = nullptr;
    vector<InfoProcesso> processosInfo;

    if (argc >= 2) {
        // ---- Modo arquivo ----
        cout << "\n[Modo Arquivo] Carregando: " << argv[1] << "\n";
        if (!carregarArquivo(argv[1], simulador, processosInfo)) return 1;

        // Ordena por tempo de chegada
        sort(processosInfo.begin(), processosInfo.end());

        for (size_t i = 0; i < processosInfo.size(); i++) {
            simulador->adicionarProcesso(
                (int)i,
                processosInfo[i].nome,
                processosInfo[i].paginas,
                processosInfo[i].tempoChegada
            );
        }

    }
    else {
        // ---- Modo interativo ----
        cout << "\n[Modo Interativo] (use ./simulador config.txt para carregar arquivo)\n\n";

        int quantum, frames, penalidade, numProcessos;
        cout << "Quantum Round Robin     : "; cin >> quantum;
        cout << "Frames de RAM           : "; cin >> frames;
        cout << "Penalidade I/O (ticks)  : "; cin >> penalidade;
        cout << "Número de processos     : "; cin >> numProcessos;

        simulador = new Simulador(quantum, frames, penalidade);

        cin.ignore();
        cout << "\nDigite as páginas de cada processo separadas por vírgula (ex: 1,2,5)\n\n";

        for (int i = 0; i < numProcessos; i++) {
            string nome = "P" + to_string(i);
            int chegada;
            string paginasStr;

            cout << "Tempo de chegada de " << nome << " : "; cin >> chegada;
            cin.ignore();
            cout << "Páginas de " << nome << "          : "; getline(cin, paginasStr);

            vector<int> paginas = parsearPaginas(paginasStr);
            if (paginas.empty()) {
                cout << "AVISO: " << nome << " sem páginas, ignorado.\n";
                continue;
            }
            simulador->adicionarProcesso(i, nome, paginas, chegada);
        }
    }

    simulador->executar();

    cout << "\n========== FIM DA SIMULAÇÃO ==========\n\n";
    delete simulador;
    return 0;
}