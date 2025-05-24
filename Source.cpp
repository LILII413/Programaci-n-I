#include <iostream>
#include <fstream>
#include <string>
#include <map>
#include <sstream>
#include <vector>
#include <algorithm>
#include <filesystem>
#include <curl/curl.h>
#include <nlohmann/json.hpp>
#include <msclr/marshal_cppstd.h>
#include <windows.h>
#include <locale>

using namespace std;
using namespace System;
using namespace System::Data::SqlClient;
using namespace msclr::interop;
namespace fs = std::filesystem;
using json = nlohmann::json;

#ifdef min
#undef min
#endif
#ifdef max
#undef max
#endif

// --- Eliminar acentos, convertir a minúsculas y quitar signos de puntuación ---
string normalizarTexto(const string& textoOriginal) {
    string texto = textoOriginal;
    transform(texto.begin(), texto.end(), texto.begin(), ::tolower);

    string resultado;
    for (char c : texto) {
        switch (c) {
        case 'á': case 'à': case 'ä': case 'â': resultado += 'a'; break;
        case 'é': case 'è': case 'ë': case 'ê': resultado += 'e'; break;
        case 'í': case 'ì': case 'ï': case 'î': resultado += 'i'; break;
        case 'ó': case 'ò': case 'ö': case 'ô': resultado += 'o'; break;
        case 'ú': case 'ù': case 'ü': case 'û': resultado += 'u'; break;
        case 'ñ': resultado += 'n'; break;
        default:
            if (isalnum(static_cast<unsigned char>(c)) || c == ' ') {
                resultado += c;
            }
        }
    }

    return resultado;
}

size_t WriteCallback(void* contents, size_t size, size_t nmemb, std::string* userp) {
    size_t totalSize = size * nmemb;
    userp->append((char*)contents, totalSize);
    return totalSize;
}

void cargarArchivo(map<string, string>& conocimiento, const string& archivoNombre) {
    conocimiento.clear();
    ifstream archivo(archivoNombre);
    if (!archivo.is_open()) {
        cout << "Error al abrir el archivo conocimiento.txt" << endl;
        return;
    }

    string linea;
    while (getline(archivo, linea)) {
        size_t separador = linea.find('|');
        if (separador != string::npos) {
            string pregunta = linea.substr(0, separador);
            string respuesta = linea.substr(separador + 1);
            conocimiento[normalizarTexto(pregunta)] = respuesta;
        }
    }
    archivo.close();
}

fs::file_time_type obtenerUltimaModificacion(const string& archivoNombre) {
    return fs::last_write_time(archivoNombre);
}

string buscarEnMap(const map<string, string>& conocimiento, const string& pregunta) {
    auto it = conocimiento.find(normalizarTexto(pregunta));
    return (it != conocimiento.end()) ? it->second : "";
}

void conectarBaseDeDatos(map<string, string>& conocimiento) {
    String^ cadenaConexion = "Data Source=EMANUEL\\SQLEXPRESS;Initial Catalog=GastronomiaDB;Integrated Security=True;";
    SqlConnection^ conexion = gcnew SqlConnection(cadenaConexion);

    try {
        conexion->Open();
        Console::WriteLine("Conexion exitosa a la base de datos de gastronomia.");

        String^ consulta = "SELECT Pregunta, Respuesta FROM Recetas";
        SqlCommand^ comando = gcnew SqlCommand(consulta, conexion);
        SqlDataReader^ lector = comando->ExecuteReader();

        while (lector->Read()) {
            String^ pregunta = lector->GetString(0);
            String^ respuesta = lector->GetString(1);

            string pregunta_str = marshal_as<std::string>(pregunta);
            string respuesta_str = marshal_as<std::string>(respuesta);
            conocimiento[normalizarTexto(pregunta_str)] = respuesta_str;
        }
        conexion->Close();
    }
    catch (Exception^ ex) {
        Console::WriteLine("Error en la conexion: {0}", ex->Message);
    }
}

string consultarChatGPT(const string& pregunta, const string& apiKey) {
    CURL* curl;
    CURLcode res;
    string readBuffer;

    curl_global_init(CURL_GLOBAL_ALL);
    curl = curl_easy_init();

    if (curl) {
        struct curl_slist* headers = NULL;
        headers = curl_slist_append(headers, ("Authorization: Bearer " + apiKey).c_str());
        headers = curl_slist_append(headers, "Content-Type: application/json");

        string postFields = R"({
            "model": "gpt-4",
            "messages": [{"role": "user", "content": ")" + pregunta + R"("}]
        })";

        curl_easy_setopt(curl, CURLOPT_URL, "https://api.openai.com/v1/chat/completions");
        curl_easy_setopt(curl, CURLOPT_HTTPHEADER, headers);
        curl_easy_setopt(curl, CURLOPT_POSTFIELDS, postFields.c_str());
        curl_easy_setopt(curl, CURLOPT_WRITEFUNCTION, WriteCallback);
        curl_easy_setopt(curl, CURLOPT_WRITEDATA, &readBuffer);
        curl_easy_setopt(curl, CURLOPT_SSL_VERIFYPEER, 0L);
        curl_easy_setopt(curl, CURLOPT_SSL_VERIFYHOST, 0L);

        res = curl_easy_perform(curl);
        if (res != CURLE_OK) {
            cout << "[ERROR] Falló la consulta a ChatGPT: " << curl_easy_strerror(res) << endl;
        }
        curl_easy_cleanup(curl);
        curl_slist_free_all(headers);
    }

    curl_global_cleanup();

    try {
        auto jsonResponse = json::parse(readBuffer);
        return jsonResponse["choices"][0]["message"]["content"];
    }
    catch (...) {
        return "Error al procesar la respuesta de ChatGPT.";
    }
}

int main() {
    SetConsoleOutputCP(CP_UTF8);

    const string archivo = "conocimiento.txt";
    map<string, string> conocimientoArchivo;
    map<string, string> conocimientoBD;

    cargarArchivo(conocimientoArchivo, archivo);
    auto ultimaModificacion = obtenerUltimaModificacion(archivo);
    conectarBaseDeDatos(conocimientoBD);

    HANDLE hConsole = GetStdHandle(STD_OUTPUT_HANDLE);

    SetConsoleTextAttribute(hConsole, FOREGROUND_RED | FOREGROUND_GREEN | FOREGROUND_BLUE | FOREGROUND_INTENSITY);

    cout << R"(

  ____   ___   ____     _    ____   ___ _____ ___  
 | __ ) / _ \ / ___|   / \  |  _ \ |_ _|_   _/ _ \
 |  _ \| | | || |     / _ \ | | | | | |  | || | | |
 | |_) | |_| || |___ / /_\ \| |_| | | |  | || |_| |
 |____/ \___/ \____|/_/   \_\____/ |___| |_| \___/


                                                             
)" << endl;


    SetConsoleTextAttribute(hConsole, FOREGROUND_GREEN | FOREGROUND_INTENSITY);
    cout << "Preguntame sobre recetas, tecnicas de cocina e ingredientes." << endl;

    SetConsoleTextAttribute(hConsole, FOREGROUND_RED | FOREGROUND_INTENSITY);
    cout << "Escribe 'salir' para terminar." << endl;

    SetConsoleTextAttribute(hConsole, FOREGROUND_RED | FOREGROUND_GREEN | FOREGROUND_BLUE | FOREGROUND_INTENSITY);

    string pregunta;
    string apiKey = "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";

    while (true) {
        auto nuevaModificacion = obtenerUltimaModificacion(archivo);
        if (nuevaModificacion > ultimaModificacion) {
            ultimaModificacion = nuevaModificacion;
            cargarArchivo(conocimientoArchivo, archivo);
            SetConsoleTextAttribute(hConsole, FOREGROUND_GREEN | FOREGROUND_INTENSITY);
            cout << "[INFO] Archivo conocimiento.txt actualizado." << endl;
            SetConsoleTextAttribute(hConsole, FOREGROUND_RED | FOREGROUND_GREEN | FOREGROUND_BLUE | FOREGROUND_INTENSITY);
        }

        system("cls");

        SetConsoleTextAttribute(hConsole, FOREGROUND_RED | FOREGROUND_GREEN | FOREGROUND_BLUE | FOREGROUND_INTENSITY);
        cout << R"(

  ____   ___   ____     _    ____   ___ _____ ___  
 | __ ) / _ \ / ___|   / \  |  _ \ |_ _|_   _/ _ \
 |  _ \| | | || |     / _ \ | | | | | |  | || | | |
 | |_) | |_| || |___ / /_\ \| |_| | | |  | || |_| |
 |____/ \___/ \____|/_/   \_\____/ |___| |_| \___/
                                                             
)" << endl;

        SetConsoleTextAttribute(hConsole, FOREGROUND_GREEN | FOREGROUND_INTENSITY);
        cout << "Preguntame sobre recetas, tecnicas de cocina e ingredientes." << endl;

        SetConsoleTextAttribute(hConsole, FOREGROUND_RED | FOREGROUND_INTENSITY);
        cout << "Escribe 'salir' para terminar." << endl;

        SetConsoleTextAttribute(hConsole, FOREGROUND_RED | FOREGROUND_GREEN | FOREGROUND_BLUE | FOREGROUND_INTENSITY);

        cout << "\n>>> Tu pregunta: ";
        getline(cin, pregunta);

        if (normalizarTexto(pregunta) == "salir") {
            SetConsoleTextAttribute(hConsole, FOREGROUND_GREEN | FOREGROUND_INTENSITY);
            cout << "Bot: ¡Buen provecho! " << endl;
            break;
        }

        string respuesta = buscarEnMap(conocimientoArchivo, pregunta);
        if (!respuesta.empty()) {
            SetConsoleTextAttribute(hConsole, FOREGROUND_BLUE | FOREGROUND_INTENSITY);
            cout << " Archivo: " << respuesta << endl;
            SetConsoleTextAttribute(hConsole, FOREGROUND_RED | FOREGROUND_GREEN | FOREGROUND_BLUE | FOREGROUND_INTENSITY);
            system("pause");
            continue;
        }

        respuesta = buscarEnMap(conocimientoBD, pregunta);
        if (!respuesta.empty()) {
            SetConsoleTextAttribute(hConsole, FOREGROUND_GREEN | FOREGROUND_BLUE | FOREGROUND_INTENSITY);
            cout << " BD: " << respuesta << endl;
            SetConsoleTextAttribute(hConsole, FOREGROUND_RED | FOREGROUND_GREEN | FOREGROUND_BLUE | FOREGROUND_INTENSITY);
            system("pause");
            continue;
        }

        SetConsoleTextAttribute(hConsole, FOREGROUND_RED | FOREGROUND_BLUE | FOREGROUND_INTENSITY);
        cout << " Consultando a ChatGPT..." << endl;
        SetConsoleTextAttribute(hConsole, FOREGROUND_RED | FOREGROUND_GREEN | FOREGROUND_BLUE);

        string respuestaIA = consultarChatGPT(pregunta, apiKey);

        SetConsoleTextAttribute(hConsole, FOREGROUND_RED | FOREGROUND_BLUE);
        cout << " ChatGPT: " << respuestaIA << endl;

        SetConsoleTextAttribute(hConsole, FOREGROUND_RED | FOREGROUND_GREEN | FOREGROUND_BLUE | FOREGROUND_INTENSITY);
        system("pause");
    }

    return 0;
}

