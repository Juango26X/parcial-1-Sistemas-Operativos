#include <iostream>
#include <vector>
#include <queue>
#include <fstream>
#include <sstream>
#include <algorithm>
#include <tuple>
#include <iomanip>
//juan David Vasquez Pomar
using namespace std;

class Proceso {
public:
    string nombre;
    int burst, llegada, cola_orig, prioridad_orig;
    int restante, cola_actual, espera, completado, respuesta, tat;
    bool primera_vez;
    
    Proceso(string n, int b, int ll, int c, int p) {
        nombre = n;
        burst = b;
        llegada = ll;
        cola_orig = c;  // Guardar para el archivo de salida (aunque no se usa en MLFQ)
        prioridad_orig = p;  // Guardar para el archivo de salida (aunque no se usa en MLFQ)
        restante = b;
        cola_actual = -1; // -1 = no ha llegado aún
        espera = 0;
        completado = 0;
        respuesta = -1;
        tat = 0;
        primera_vez = true;
    }
};

bool compararProcesos(const Proceso &a, const Proceso &b) {
    return tie(a.llegada, a.nombre) < tie(b.llegada, b.nombre);
}

class MLFQ {
public:
    vector<Proceso> procesos;
    queue<int> q0, q1, q2, q3;
    int tiempo;
    vector<int> quantum;
    
    MLFQ() {
        tiempo = 0;
        quantum.push_back(1);  // Q0: RR(1)
        quantum.push_back(3);  // Q1: RR(3) 
        quantum.push_back(4);  // Q2: RR(4)
        quantum.push_back(-1); // Q3: SJF (sin quantum)
        cout << "Sistema MLFQ: Q0=RR(1), Q1=RR(3), Q2=RR(4), Q3=SJF\n";
        cout << "Todos los procesos inician en Q0 (MLFQ estandar)\n\n";
    }
    
    string limpiar_espacios(const string& str) {
        size_t inicio = str.find_first_not_of(" \t\r\n");
        if (inicio == string::npos) return "";
        size_t fin = str.find_last_not_of(" \t\r\n");
        return str.substr(inicio, fin - inicio + 1);
    }
    
    void cargar(string archivo) {
        ifstream f(archivo);
        if (!f.is_open()) {
            cout << "Error: No se pudo abrir el archivo " << archivo << endl;
            return;
        }
        
        string linea;
        int linea_num = 0;
        while (getline(f, linea)) {
            linea_num++;
            
            if (!linea.empty() && linea[0] != '#') {
                stringstream ss(linea);
                string n, b, ll, c, p;
                
                if (getline(ss, n, ';') && getline(ss, b, ';') && 
                    getline(ss, ll, ';') && getline(ss, c, ';') && 
                    getline(ss, p, ';')) {
                    
                    n = limpiar_espacios(n);
                    b = limpiar_espacios(b);
                    ll = limpiar_espacios(ll);
                    c = limpiar_espacios(c);
                    p = limpiar_espacios(p);
                    
                    try {
                        int burst_time = stoi(b);
                        int arrival_time = stoi(ll);
                        int cola_orig = stoi(c);
                        int prioridad = stoi(p);
                        
                        procesos.push_back(Proceso(n, burst_time, arrival_time, cola_orig, prioridad));
                        cout << "Cargado: " << n << " BT=" << burst_time << " AT=" << arrival_time << endl;
                    } catch (const exception& e) {
                        cout << "Error en línea " << linea_num << ": " << e.what() << endl;
                    }
                }
            }
        }
        f.close();
        
        if (procesos.empty()) {
            cout << "No se cargaron procesos válidos" << endl;
            return;
        }
        
        sort(procesos.begin(), procesos.end(), compararProcesos);
        cout << "\nCargados " << procesos.size() << " procesos ordenados por AT\n" << endl;
    }
    //actualicamos a la cola q0 los procesos listos
    void mover_llegadas() {
        for (int i = 0; i < procesos.size(); i++) {
            if (procesos[i].llegada <= tiempo && procesos[i].cola_actual == -1) {
                procesos[i].cola_actual = 0;  // MLFQ: Todos empiezan en Q0
                q0.push(i);
                cout << "T" << tiempo << ": " << procesos[i].nombre << " llega a Q0" << endl;
            }
        }
    }
    //seleccionamos el proximo proceso idx a ejecutar
    int seleccionar() {
        if (!q0.empty()) {
            int idx = q0.front(); 
            q0.pop(); 
            return idx;
        }
        if (!q1.empty()) {
            int idx = q1.front(); 
            q1.pop(); 
            return idx;
        }
        if (!q2.empty()) {
            int idx = q2.front(); 
            q2.pop(); 
            return idx;
        }
        if (!q3.empty()) {
            return seleccionar_sjf();
        }
        return -1;
    }
    
    int seleccionar_sjf() {
        if (q3.empty()) return -1;
        
        vector<int> temp;
        int mejor = -1;
        
        // Extraer todos los procesos de Q3
        while (!q3.empty()) {
            int idx = q3.front();
            q3.pop();
            temp.push_back(idx);
            
            if (mejor == -1 || 
                procesos[idx].restante < procesos[mejor].restante ||
                (procesos[idx].restante == procesos[mejor].restante && 
                 procesos[idx].nombre < procesos[mejor].nombre)) {
                mejor = idx;
            }
        }
        
        // Devolver los que no son el mejor a Q3
        for (int idx : temp) {
            if (idx != mejor) {
                q3.push(idx);
            }
        }
        
        return mejor;
    }
    
    void agregar_a_cola(int idx) {
        switch (procesos[idx].cola_actual) {
            case 0: q0.push(idx); break;
            case 1: q1.push(idx); break;
            case 2: q2.push(idx); break;
            case 3: q3.push(idx); break;
        }
    }
    
    void ejecutar(int idx) {
        Proceso& p = procesos[idx];
        int cola = p.cola_actual;
        
        // Calcular tiempo de respuesta en primera ejecución
        if (p.primera_vez) {
            p.respuesta = tiempo - p.llegada;
            p.primera_vez = false;
            cout << "Tiempo de respuesta para " << p.nombre << ": " << p.respuesta << endl;
        }
        
        // Determinar tiempo de ejecución
        int exec_time;
        if (cola == 3) {
            // En Q3 (SJF), ejecuta hasta terminar
            exec_time = p.restante;
        } else {
            // En otras colas, usa el quantum
            exec_time = min(quantum[cola], p.restante);
        }
        
        cout << "T" << tiempo << "-T" << (tiempo + exec_time) 
             << ": " << p.nombre << " ejecuta " << exec_time << " unidades en Q" << cola 
             << " (restante: " << p.restante << ")" << endl;
        
        // Actualizar tiempos de espera para otros procesos que están esperando
        for (int i = 0; i < procesos.size(); i++) {
            if (i != idx && procesos[i].cola_actual >= 0 && procesos[i].restante > 0) {
                procesos[i].espera += exec_time;
            }
        }
        
        // Actualizar el proceso ejecutado
        p.restante -= exec_time;
        tiempo += exec_time;
        
        // Mover nuevas llegadas DESPUÉS de actualizar el tiempo
        mover_llegadas();
        
        if (p.restante == 0) {
            // Proceso terminado
            p.completado = tiempo;
            p.tat = p.completado - p.llegada;
            cout << "*** " << p.nombre << " TERMINADO en T" << tiempo << " ***" << endl;
        } else {
            // Proceso no terminado
            if (cola < 3 && exec_time == quantum[cola]) {
                // Usó todo su quantum, baja de nivel
                p.cola_actual = min(3, cola + 1);
                cout << p.nombre << " agota quantum, baja a Q" << p.cola_actual << endl;
            }
            // Si no usó todo el quantum (fue interrumpido) o está en Q3, permanece en la misma cola
            
            agregar_a_cola(idx);
        }
        cout << endl;
    }
    
    bool terminado() {
        // Verificar si todos los procesos han terminado
        for (const auto& p : procesos) {
            if (p.restante > 0) return false;
        }
        return q0.empty() && q1.empty() && q2.empty() && q3.empty();
    }
    
    bool hay_procesos_por_llegar() {
        for (const auto& p : procesos) {
            if (p.llegada > tiempo) return true;
        }
        return false;
    }
    
    void simular() {
        cout << "=== INICIANDO SIMULACIÓN MLFQ ===" << endl;
        int iteraciones = 0;
        const int MAX_ITERACIONES = 1000;
        
        while (!terminado() && iteraciones < MAX_ITERACIONES) {
            iteraciones++;
            
            cout << "\n--- TIEMPO " << tiempo << " ---" << endl;
            
            // Mostrar estado
            cout << "Colas: Q0(" << q0.size() << ") Q1(" << q1.size() 
                 << ") Q2(" << q2.size() << ") Q3(" << q3.size() << ")" << endl;
            
            mover_llegadas();
            int idx = seleccionar();
            
            if (idx == -1) {
                if (hay_procesos_por_llegar()) {
                    cout << "CPU inactiva, avanzando tiempo..." << endl;
                    tiempo++;
                    // Importante: actualizar espera de procesos que ya están en el sistema esperando
                    for (auto& p : procesos) {
                        if (p.cola_actual >= 0 && p.restante > 0) {
                            p.espera++;
                        }
                    }
                } else {
                    cout << "No hay más procesos por llegar ni en colas" << endl;
                    break;
                }
            } else {
                ejecutar(idx);
            }
            
            // Verificar progreso cada 50 iteraciones
            if (iteraciones % 50 == 0) {
                cout << "*** Iteración " << iteraciones << " - Tiempo: " << tiempo << " ***" << endl;
            }
        }
        
        if (iteraciones >= MAX_ITERACIONES) {
            cout << "*** ADVERTENCIA: Simulación detenida por límite de iteraciones ***" << endl;
        }
        
        cout << "\n=== SIMULACIÓN TERMINADA ===" << endl;
        cout << "Iteraciones totales: " << iteraciones << endl;
        cout << "Tiempo final: " << tiempo << endl;
    }
    
    void mostrar() {
        cout << "\n=== RESULTADOS FINALES ===" << endl;
        cout << "Proceso  BT  AT  WT  CT  RT  TAT" << endl;
        cout << "--------------------------------" << endl;
        
        // Ordenar por nombre para mostrar
        sort(procesos.begin(), procesos.end(), [](const Proceso &a, const Proceso &b) {
            return a.nombre < b.nombre;
        });
        
        double sum_wt = 0, sum_ct = 0, sum_rt = 0, sum_tat = 0;
        int completados = 0;
        
        for (const auto& p : procesos) {
            cout << p.nombre << "\t " << p.burst << "   " << p.llegada << "   ";
            
            if (p.completado > 0) {
                cout << p.espera << "   " << p.completado << "   " << p.respuesta << "    " << p.tat;
                sum_wt += p.espera; 
                sum_ct += p.completado;
                sum_rt += p.respuesta; 
                sum_tat += p.tat;
                completados++;
            } else {
                cout << "NO TERMINADO";
            }
            cout << endl;
        }
        
        if (completados > 0) {
            cout << "--------------------------------" << endl;
            cout << fixed << setprecision(1);
            cout << "WT=" << sum_wt/completados << " CT=" << sum_ct/completados 
                 << " RT=" << sum_rt/completados << " TAT=" << sum_tat/completados << endl;
        }
    }
    
    void guardar(string archivo) {
        string archivo_salida = "output_" + archivo;
        ofstream f(archivo_salida);
        
        if (!f.is_open()) {
            cout << "Error: No se pudo crear " << archivo_salida << endl;
            return;
        }
        
        f << "# Resultados MLFQ para: " << archivo << endl;
        f << "# etiqueta; BT; AT; Q; Pr; WT; CT; RT; TAT" << endl;
        
        // Ordenar por nombre para guardar
        sort(procesos.begin(), procesos.end(), [](const Proceso &a, const Proceso &b) {
            return a.nombre < b.nombre;
        });
        
        double sum_wt = 0, sum_ct = 0, sum_rt = 0, sum_tat = 0;
        int completados = 0;
        
        for (const auto& p : procesos) {
            f << p.nombre << ";" << p.burst << ";" << p.llegada << ";" 
              << p.cola_orig << ";" << p.prioridad_orig << ";";
              
            if (p.completado > 0) {
                f << p.espera << ";" << p.completado << ";" << p.respuesta << ";" << p.tat;
                sum_wt += p.espera; 
                sum_ct += p.completado;
                sum_rt += p.respuesta; 
                sum_tat += p.tat;
                completados++;
            } else {
                f << "NO_TERM;NO_TERM;NO_TERM;NO_TERM";
            }
            f << endl;
        }
        
        if (completados > 0) {
            f << fixed << setprecision(1);
            f << "# PROMEDIOS: WT=" << sum_wt/completados << "; CT=" << sum_ct/completados 
              << "; RT=" << sum_rt/completados << "; TAT=" << sum_tat/completados << endl;
        }
        
        f.close();
        cout << "Resultados guardados en: " << archivo_salida << endl;
    }
};

int main() {
    cout << "=== SIMULADOR MLFQ ===" << endl;
    
    MLFQ simulador;
    
    cout << "Ingrese el nombre del archivo: ";
    string archivo;
    getline(cin, archivo);
    if (archivo.empty()) {
        archivo = "mlq001.txt";
        cout << "Usando archivo por defecto: " << archivo << endl;
    }
    
    simulador.cargar(archivo);
    
    if (simulador.procesos.empty()) {
        cout << "No se cargaron procesos. Terminando..." << endl;
        return 1;
    }
    
    simulador.simular();
    simulador.mostrar();
    simulador.guardar(archivo);
    
    cout << "\nPresione Enter para salir...";
    cin.get();
    
    return 0;
}