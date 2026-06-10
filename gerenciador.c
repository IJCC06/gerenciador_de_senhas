// Define Unicode antes de qualquer include
#define UNICODE
#define _UNICODE

#include <windows.h>
#include <stdio.h>
#include <string.h>
#include <stdlib.h>
#include <time.h>
#include <commctrl.h>

// Estrutura para armazenar usuários e senhas (dados internos em UTF-8 para o arquivo)
typedef struct {
    char usuario[64];
    char senha[64];
    char dataCriacao[32];
} Credencial;

#define MAX_CREDENCIAIS 100
#define ARQUIVO_DADOS L"credenciais.dat"

static Credencial credenciais[MAX_CREDENCIAIS];
static int numCredenciais = 0;
static HWND g_hwndLista = NULL;
static int temaEscuro = 0;

// Persistência de dados (usa _wfopen para wide)
int SalvarDados() {
    FILE* arquivo = _wfopen(ARQUIVO_DADOS, L"wb");
    if (!arquivo) return 0;
    fwrite(&numCredenciais, sizeof(int), 1, arquivo);
    fwrite(credenciais, sizeof(Credencial), numCredenciais, arquivo);
    fclose(arquivo);
    return 1;
}

int CarregarDados() {
    FILE* arquivo = _wfopen(ARQUIVO_DADOS, L"rb");
    if (!arquivo) return 0;
    fread(&numCredenciais, sizeof(int), 1, arquivo);
    if (numCredenciais < 0 || numCredenciais > MAX_CREDENCIAIS) {
        numCredenciais = 0;
        fclose(arquivo);
        return 0;
    }
    fread(credenciais, sizeof(Credencial), numCredenciais, arquivo);
    fclose(arquivo);
    return 1;
}

void InicializarDadosExemplo() {
    if (CarregarDados()) return;
    strcpy(credenciais[0].usuario, "admin");
    strcpy(credenciais[0].senha, "123456");
    strcpy(credenciais[1].usuario, "joao");
    strcpy(credenciais[1].senha, "senha123");
    strcpy(credenciais[2].usuario, "maria");
    strcpy(credenciais[2].senha, "abc456");
    strcpy(credenciais[3].usuario, "pedro");
    strcpy(credenciais[3].senha, "pedro2023");
    numCredenciais = 4;
    for (int i = 0; i < numCredenciais; i++)
        strcpy(credenciais[i].dataCriacao, "2025-01-01");
    SalvarDados();
}

void ObterDataAtual(char *buffer, int size) {
    SYSTEMTIME st;
    GetLocalTime(&st);
    sprintf(buffer, "%04d-%02d-%02d", st.wYear, st.wMonth, st.wDay);
}

int UsuarioDisponivel(const char* usuario, int indiceIgnorar) {
    for (int i = 0; i < numCredenciais; i++)
        if (i != indiceIgnorar && strcmp(credenciais[i].usuario, usuario) == 0)
            return 0;
    return 1;
}

void GerarSenhaForte(char *senha, int comprimento) {
    const char caracteres[] = "ABCDEFGHIJKLMNOPQRSTUVWXYZabcdefghijklmnopqrstuvwxyz0123456789!@#$%^&*()";
    int numCaracteres = sizeof(caracteres) - 1;
    srand((unsigned int)time(NULL));
    for (int i = 0; i < comprimento; i++)
        senha[i] = caracteres[rand() % numCaracteres];
    senha[comprimento] = '\0';
}

int AvaliarForcaSenha(const char* senha) {
    int len = strlen(senha);
    int maiuscula = 0, minuscula = 0, numero = 0, especial = 0;
    for (int i = 0; i < len; i++) {
        if (senha[i] >= 'A' && senha[i] <= 'Z') maiuscula = 1;
        else if (senha[i] >= 'a' && senha[i] <= 'z') minuscula = 1;
        else if (senha[i] >= '0' && senha[i] <= '9') numero = 1;
        else especial = 1;
    }
    int variedade = maiuscula + minuscula + numero + especial;
    if (len < 6 || variedade < 2) return 0;
    if (len >= 8 && variedade >= 3) return 2;
    return 1;
}

void Notificar(HWND hwnd, const wchar_t* mensagem, int tipo) {
    UINT icone = (tipo == 1) ? MB_ICONWARNING : (tipo == 2) ? MB_ICONERROR : MB_ICONINFORMATION;
    MessageBoxW(hwnd, mensagem, L"Notificação", MB_OK | icone);
}

// Converte UTF-8 para wchar_t (para exibir nos controles)
void Utf8ToWide(const char* utf8, wchar_t* wide, int wideSize) {
    MultiByteToWideChar(CP_UTF8, 0, utf8, -1, wide, wideSize);
}

// Converte wchar_t para UTF-8 (para salvar no arquivo)
void WideToUtf8(const wchar_t* wide, char* utf8, int utf8Size) {
    WideCharToMultiByte(CP_UTF8, 0, wide, -1, utf8, utf8Size, NULL, NULL);
}

// Adiciona item ao ListView (usando macros Unicode)
void AdicionarItemListView(HWND hListView, int index, int id, const char* usuario, const char* data) {
    LVITEMW item = {0};
    wchar_t idStr[10];
    wsprintfW(idStr, L"%d", id);
    
    wchar_t usuarioW[64], dataW[32];
    Utf8ToWide(usuario, usuarioW, 64);
    Utf8ToWide(data, dataW, 32);
    
    item.mask = LVIF_TEXT;
    item.iItem = index;
    item.iSubItem = 0;
    item.pszText = idStr;
    ListView_InsertItem(hListView, &item);
    
    ListView_SetItemText(hListView, index, 1, usuarioW);
    ListView_SetItemText(hListView, index, 2, dataW);
}

// Janela de gerenciamento
LRESULT CALLBACK ListaProc(HWND hwnd, UINT msg, WPARAM wParam, LPARAM lParam) {
    static HWND hListView, hEditBusca, hEditUser, hEditPass;
    static int selectedIndex = -1;
    static wchar_t filtro[64] = L"";

    switch (msg) {
        case WM_CREATE: {
            hListView = CreateWindowExW(0, WC_LISTVIEWW, L"", WS_CHILD | WS_VISIBLE | LVS_REPORT | LVS_SINGLESEL,
                                       10, 10, 400, 200, hwnd, (HMENU)1001, NULL, NULL);
            LVCOLUMNW col = {0};
            col.mask = LVCF_TEXT | LVCF_WIDTH;
            col.pszText = L"ID"; col.cx = 40; ListView_InsertColumn(hListView, 0, &col);
            col.pszText = L"Usuário"; col.cx = 120; ListView_InsertColumn(hListView, 1, &col);
            col.pszText = L"Data Criação"; col.cx = 100; ListView_InsertColumn(hListView, 2, &col);

            CreateWindowW(L"STATIC", L"Buscar:", WS_CHILD | WS_VISIBLE, 420, 10, 50, 20, hwnd, NULL, NULL, NULL);
            hEditBusca = CreateWindowW(L"EDIT", L"", WS_CHILD | WS_VISIBLE | WS_BORDER, 480, 8, 120, 24, hwnd, (HMENU)1002, NULL, NULL);
            CreateWindowW(L"BUTTON", L"Filtrar", WS_CHILD | WS_VISIBLE, 610, 8, 70, 24, hwnd, (HMENU)1003, NULL, NULL);

            CreateWindowW(L"STATIC", L"Usuário:", WS_CHILD | WS_VISIBLE, 420, 50, 100, 20, hwnd, NULL, NULL, NULL);
            hEditUser = CreateWindowW(L"EDIT", L"", WS_CHILD | WS_VISIBLE | WS_BORDER, 420, 70, 180, 24, hwnd, (HMENU)1004, NULL, NULL);
            CreateWindowW(L"STATIC", L"Senha:", WS_CHILD | WS_VISIBLE, 420, 100, 100, 20, hwnd, NULL, NULL, NULL);
            hEditPass = CreateWindowW(L"EDIT", L"", WS_CHILD | WS_VISIBLE | WS_BORDER, 420, 120, 180, 24, hwnd, (HMENU)1005, NULL, NULL);

            CreateWindowW(L"BUTTON", L"Gerar Senha", WS_CHILD | WS_VISIBLE, 610, 120, 80, 24, hwnd, (HMENU)1006, NULL, NULL);
            CreateWindowW(L"BUTTON", L"Ver Força", WS_CHILD | WS_VISIBLE, 420, 150, 100, 24, hwnd, (HMENU)1007, NULL, NULL);
            CreateWindowW(L"BUTTON", L"Salvar Alterações", WS_CHILD | WS_VISIBLE, 530, 150, 120, 24, hwnd, (HMENU)1008, NULL, NULL);
            CreateWindowW(L"BUTTON", L"Excluir", WS_CHILD | WS_VISIBLE, 660, 150, 80, 24, hwnd, (HMENU)1009, NULL, NULL);
            CreateWindowW(L"BUTTON", L"Alternar Tema", WS_CHILD | WS_VISIBLE, 420, 190, 100, 24, hwnd, (HMENU)1010, NULL, NULL);

            for (int i = 0; i < numCredenciais; i++)
                AdicionarItemListView(hListView, i, i+1, credenciais[i].usuario, credenciais[i].dataCriacao);
            break;
        }
        case WM_COMMAND: {
            if (LOWORD(wParam) == 1003) { // Filtrar
                GetWindowTextW(hEditBusca, filtro, 63);
                ListView_DeleteAllItems(hListView);
                int idx = 0;
                for (int i = 0; i < numCredenciais; i++) {
                    wchar_t usuarioW[64];
                    Utf8ToWide(credenciais[i].usuario, usuarioW, 64);
                    if (wcslen(filtro) == 0 || wcsstr(usuarioW, filtro) != NULL) {
                        AdicionarItemListView(hListView, idx, i+1, credenciais[i].usuario, credenciais[i].dataCriacao);
                        idx++;
                    }
                }
            }
            else if (LOWORD(wParam) == 1006) { // Gerar senha
                char novaSenha[64];
                GerarSenhaForte(novaSenha, 12);
                wchar_t novaSenhaW[64];
                Utf8ToWide(novaSenha, novaSenhaW, 64);
                SetWindowTextW(hEditPass, novaSenhaW);
                Notificar(hwnd, L"Senha forte gerada!", 0);
            }
            else if (LOWORD(wParam) == 1007) { // Ver força
                wchar_t senhaW[64];
                GetWindowTextW(hEditPass, senhaW, 63);
                char senha[64];
                WideToUtf8(senhaW, senha, 64);
                int forca = AvaliarForcaSenha(senha);
                const wchar_t* msg = (forca == 0) ? L"Senha FRACA. Use letras, números e símbolos." :
                                      (forca == 1) ? L"Senha MÉDIA. Considere aumentar a variedade." :
                                                     L"Senha FORTE!";
                Notificar(hwnd, msg, forca == 0 ? 1 : 0);
            }
            else if (LOWORD(wParam) == 1008) { // Salvar
                if (selectedIndex >= 0 && selectedIndex < numCredenciais) {
                    wchar_t novoUsuarioW[64], novaSenhaW[64];
                    char novoUsuario[64], novaSenha[64];
                    GetWindowTextW(hEditUser, novoUsuarioW, 63);
                    GetWindowTextW(hEditPass, novaSenhaW, 63);
                    WideToUtf8(novoUsuarioW, novoUsuario, 64);
                    WideToUtf8(novaSenhaW, novaSenha, 64);
                    
                    if (strlen(novoUsuario) == 0 || strlen(novaSenha) == 0) {
                        Notificar(hwnd, L"Preencha ambos os campos.", 1); break;
                    }
                    if (!UsuarioDisponivel(novoUsuario, selectedIndex)) {
                        Notificar(hwnd, L"Nome de usuário já existe.", 1); break;
                    }
                    if (AvaliarForcaSenha(novaSenha) == 0) {
                        if (MessageBoxW(hwnd, L"Senha fraca. Continuar?", L"Aviso", MB_YESNO) != IDYES)
                            break;
                    }
                    strcpy(credenciais[selectedIndex].usuario, novoUsuario);
                    strcpy(credenciais[selectedIndex].senha, novaSenha);
                    ObterDataAtual(credenciais[selectedIndex].dataCriacao, 32);
                    SalvarDados();
                    Notificar(hwnd, L"Credencial atualizada.", 0);
                    SendMessageW(hwnd, WM_COMMAND, 1003, 0);
                } else Notificar(hwnd, L"Selecione um usuário.", 1);
            }
            else if (LOWORD(wParam) == 1009) { // Excluir
                if (selectedIndex >= 0 && selectedIndex < numCredenciais) {
                    wchar_t msg[256];
                    wsprintfW(msg, L"Excluir '%hs'?", credenciais[selectedIndex].usuario);
                    if (MessageBoxW(hwnd, msg, L"Confirmar", MB_YESNO) == IDYES) {
                        for (int i = selectedIndex; i < numCredenciais-1; i++)
                            credenciais[i] = credenciais[i+1];
                        numCredenciais--;
                        SalvarDados();
                        Notificar(hwnd, L"Usuário excluído.", 0);
                        selectedIndex = -1;
                        SetWindowTextW(hEditUser, L"");
                        SetWindowTextW(hEditPass, L"");
                        SendMessageW(hwnd, WM_COMMAND, 1003, 0);
                    }
                } else Notificar(hwnd, L"Selecione um usuário.", 1);
            }
            else if (LOWORD(wParam) == 1010) { // Alternar tema
                temaEscuro = !temaEscuro;
                HBRUSH novaCor = temaEscuro ? CreateSolidBrush(RGB(45, 45, 45)) : GetStockObject(WHITE_BRUSH);
                SetClassLongPtrW(hwnd, GCLP_HBRBACKGROUND, (LONG_PTR)novaCor);
                InvalidateRect(hwnd, NULL, TRUE);
                Notificar(hwnd, temaEscuro ? L"Tema escuro ativado." : L"Tema claro ativado.", 0);
            }
            break;
        }
        case WM_NOTIFY: {
            NMHDR* nmhdr = (NMHDR*)lParam;
            if (nmhdr->idFrom == 1001 && nmhdr->code == LVN_ITEMCHANGED) {
                NMLISTVIEW* nmlv = (NMLISTVIEW*)lParam;
                if (nmlv->uNewState & LVIS_SELECTED) {
                    int itemVisivel = nmlv->iItem;
                    if (itemVisivel >= 0) {
                        wchar_t filtroAtual[64];
                        GetWindowTextW(hEditBusca, filtroAtual, 63);
                        int idxReal = -1, cont = 0;
                        for (int i = 0; i < numCredenciais; i++) {
                            wchar_t usuarioW[64];
                            Utf8ToWide(credenciais[i].usuario, usuarioW, 64);
                            if (wcslen(filtroAtual) == 0 || wcsstr(usuarioW, filtroAtual) != NULL) {
                                if (cont == itemVisivel) { idxReal = i; break; }
                                cont++;
                            }
                        }
                        if (idxReal != -1) {
                            selectedIndex = idxReal;
                            wchar_t usuarioW[64], senhaW[64];
                            Utf8ToWide(credenciais[selectedIndex].usuario, usuarioW, 64);
                            Utf8ToWide(credenciais[selectedIndex].senha, senhaW, 64);
                            SetWindowTextW(hEditUser, usuarioW);
                            SetWindowTextW(hEditPass, senhaW);
                        }
                    }
                }
            }
            break;
        }
        case WM_CLOSE:
            DestroyWindow(hwnd);
            g_hwndLista = NULL;
            break;
        default:
            return DefWindowProcW(hwnd, msg, wParam, lParam);
    }
    return 0;
}

void MostrarJanelaLista(HINSTANCE hInstance) {
    if (g_hwndLista) { SetForegroundWindow(g_hwndLista); return; }
    INITCOMMONCONTROLSEX icex = {sizeof(INITCOMMONCONTROLSEX), ICC_LISTVIEW_CLASSES};
    InitCommonControlsEx(&icex);
    WNDCLASSW wc = {0};
    wc.lpfnWndProc = ListaProc;
    wc.hInstance = hInstance;
    wc.hbrBackground = (HBRUSH)(COLOR_WINDOW + 1);
    wc.lpszClassName = L"ListaWindow";
    RegisterClassW(&wc);
    g_hwndLista = CreateWindowExW(0, L"ListaWindow", L"Gerenciar Credenciais",
                                  WS_OVERLAPPEDWINDOW | WS_VISIBLE,
                                  CW_USEDEFAULT, CW_USEDEFAULT, 760, 280,
                                  NULL, NULL, hInstance, NULL);
}

int AdicionarCredencial(const char* usuario, const char* senha) {
    if (numCredenciais >= MAX_CREDENCIAIS) return -1;
    if (!UsuarioDisponivel(usuario, -1)) return 0;
    strcpy(credenciais[numCredenciais].usuario, usuario);
    strcpy(credenciais[numCredenciais].senha, senha);
    ObterDataAtual(credenciais[numCredenciais].dataCriacao, 32);
    numCredenciais++;
    return SalvarDados() ? 1 : -2;
}

// Janela principal
LRESULT CALLBACK WndProc(HWND hwnd, UINT msg, WPARAM wParam, LPARAM lParam) {
    static HWND hEditUser, hEditPass;
    static HINSTANCE hInst;
    switch (msg) {
        case WM_CREATE: {
            hInst = ((LPCREATESTRUCTW)lParam)->hInstance;
            CreateWindowW(L"STATIC", L"Usuário:", WS_CHILD | WS_VISIBLE, 20, 20, 100, 20, hwnd, NULL, NULL, NULL);
            hEditUser = CreateWindowW(L"EDIT", L"", WS_CHILD | WS_VISIBLE | WS_BORDER, 120, 18, 140, 24, hwnd, NULL, NULL, NULL);
            CreateWindowW(L"STATIC", L"Senha:", WS_CHILD | WS_VISIBLE, 20, 60, 100, 20, hwnd, NULL, NULL, NULL);
            hEditPass = CreateWindowW(L"EDIT", L"", WS_CHILD | WS_VISIBLE | WS_BORDER | ES_PASSWORD, 120, 58, 140, 24, hwnd, NULL, NULL, NULL);
            CreateWindowW(L"BUTTON", L"Entrar", WS_CHILD | WS_VISIBLE, 40, 100, 80, 30, hwnd, (HMENU)1, NULL, NULL);
            CreateWindowW(L"BUTTON", L"Ver Todos", WS_CHILD | WS_VISIBLE, 140, 100, 80, 30, hwnd, (HMENU)2, NULL, NULL);
            CreateWindowW(L"BUTTON", L"Adicionar", WS_CHILD | WS_VISIBLE, 240, 100, 80, 30, hwnd, (HMENU)3, NULL, NULL);
            CreateWindowW(L"BUTTON", L"Gerar Senha", WS_CHILD | WS_VISIBLE, 120, 140, 100, 30, hwnd, (HMENU)4, NULL, NULL);
            CreateWindowW(L"BUTTON", L"Ver Força", WS_CHILD | WS_VISIBLE, 230, 140, 80, 30, hwnd, (HMENU)5, NULL, NULL);
            break;
        }
        case WM_COMMAND: {
            wchar_t usuarioW[64], senhaW[64];
            char usuario[64], senha[64];
            GetWindowTextW(hEditUser, usuarioW, 63);
            GetWindowTextW(hEditPass, senhaW, 63);
            WideToUtf8(usuarioW, usuario, 64);
            WideToUtf8(senhaW, senha, 64);
            
            if (LOWORD(wParam) == 1) { // Entrar
                int ok = 0;
                for (int i = 0; i < numCredenciais; i++)
                    if (strcmp(credenciais[i].usuario, usuario) == 0 && strcmp(credenciais[i].senha, senha) == 0)
                        ok = 1;
                Notificar(hwnd, ok ? L"Login bem-sucedido!" : L"Usuário ou senha inválidos!", ok ? 0 : 2);
            }
            else if (LOWORD(wParam) == 2) MostrarJanelaLista(hInst);
            else if (LOWORD(wParam) == 3) { // Adicionar
                if (strlen(usuario) == 0 || strlen(senha) == 0) Notificar(hwnd, L"Preencha ambos os campos.", 1);
                else if (!UsuarioDisponivel(usuario, -1)) Notificar(hwnd, L"Nome de usuário já existe.", 1);
                else {
                    int forca = AvaliarForcaSenha(senha);
                    if (forca == 0 && MessageBoxW(hwnd, L"Senha fraca. Adicionar mesmo assim?", L"Aviso", MB_YESNO) != IDYES) break;
                    int res = AdicionarCredencial(usuario, senha);
                    if (res == 1) {
                        Notificar(hwnd, L"Usuário adicionado.", 0);
                        SetWindowTextW(hEditUser, L""); SetWindowTextW(hEditPass, L"");
                    } else if (res == -2) Notificar(hwnd, L"Erro ao salvar.", 2);
                    else Notificar(hwnd, L"Banco cheio.", 2);
                }
            }
            else if (LOWORD(wParam) == 4) { // Gerar senha
                char nova[64]; GerarSenhaForte(nova, 12);
                wchar_t novaW[64];
                Utf8ToWide(nova, novaW, 64);
                SetWindowTextW(hEditPass, novaW);
                Notificar(hwnd, L"Senha gerada!", 0);
            }
            else if (LOWORD(wParam) == 5) { // Ver força
                int forca = AvaliarForcaSenha(senha);
                const wchar_t* msg = (forca == 0) ? L"Senha FRACA." : (forca == 1) ? L"Senha MÉDIA." : L"Senha FORTE!";
                Notificar(hwnd, msg, forca == 0 ? 1 : 0);
            }
            break;
        }
        case WM_CLOSE:
            SalvarDados();
            DestroyWindow(hwnd);
            PostQuitMessage(0);
            break;
        default:
            return DefWindowProcW(hwnd, msg, wParam, lParam);
    }
    return 0;
}

int WINAPI wWinMain(HINSTANCE hInstance, HINSTANCE hPrevInstance, LPWSTR lpCmdLine, int nCmdShow) {
    srand((unsigned int)time(NULL));
    InicializarDadosExemplo();
    
    WNDCLASSW wc = {0};
    wc.lpfnWndProc = WndProc;
    wc.hInstance = hInstance;
    wc.hbrBackground = (HBRUSH)(COLOR_WINDOW + 1);
    wc.lpszClassName = L"PasswordManager";
    RegisterClassW(&wc);
    
    HWND hwnd = CreateWindowExW(0, L"PasswordManager", L"Gerenciador de Senhas",
                                WS_OVERLAPPEDWINDOW | WS_VISIBLE,
                                CW_USEDEFAULT, CW_USEDEFAULT, 380, 230,
                                NULL, NULL, hInstance, NULL);
    
    MSG msg;
    while (GetMessageW(&msg, NULL, 0, 0)) {
        TranslateMessage(&msg);
        DispatchMessageW(&msg);
    }
    return 0;
}