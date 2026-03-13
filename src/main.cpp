#include <iostream>
#include <string>
#include <curl/curl.h>
#include <unistd.h>
#include <limits.h>
#include <pwd.h>
#include <sys/utsname.h>

// Gets the machine's network name
std::string get_hostname() {
    char hostname[HOST_NAME_MAX];
    gethostname(hostname, HOST_NAME_MAX);
    return std::string(hostname);
}

// Gets the current user running the agent
std::string get_username() {
    uid_t uid = geteuid();
    struct passwd *pw = getpwuid(uid);
    return (pw) ? std::string(pw->pw_name) : "unknown";
}

// NEW: Pulls Kernel and Architecture info
struct SysInfo {
    std::string os_version;
    std::string arch;
};

SysInfo get_sys_info() {
    struct utsname buffer;
    if (uname(&buffer) != 0) {
        return {"unknown", "unknown"};
    }
    // 'release' is the kernel version, 'machine' is the architecture (x86_64)
    return { std::string(buffer.sysname) + " " + std::string(buffer.release), std::string(buffer.machine) };
}

void send_beacon(const std::string& url, const std::string& agent_id, const SysInfo& info) {
    CURL* curl = curl_easy_init();
    if(curl) {
        struct curl_slist *headers = NULL;
        headers = curl_slist_append(headers, ("X-Agent-ID: " + agent_id).c_str());
        
        // NEW: Send discovery data in headers
        headers = curl_slist_append(headers, ("X-OS-Version: " + info.os_version).c_str());
        headers = curl_slist_append(headers, ("X-Arch: " + info.arch).c_str());

        curl_easy_setopt(curl, CURLOPT_URL, url.c_str());
        curl_easy_setopt(curl, CURLOPT_POST, 1L);
        curl_easy_setopt(curl, CURLOPT_HTTPHEADER, headers);
        curl_easy_setopt(curl, CURLOPT_POSTFIELDS, ""); 

        CURLcode res = curl_easy_perform(curl);
        if(res != CURLE_OK) {
            std::cerr << "[-] Beacon failed: " << curl_easy_strerror(res) << std::endl;
        } else {
            std::cout << "[+] Beacon sent (ID: " << agent_id << " | OS: " << info.os_version << ")" << std::endl;
        }

        curl_slist_free_all(headers);
        curl_easy_cleanup(curl);
    }
}

int main() {
    std::string server_url = "http://localhost:8080/checkin";
    std::string my_id = get_hostname() + "-" + get_username();
    
    // Collect system info once at startup
    SysInfo info = get_sys_info();

    std::cout << "⚡ Hermes Agent Identity: " << my_id << std::endl;
    std::cout << "📋 Discovery: " << info.os_version << " (" << info.arch << ")" << std::endl;

    while(true) {
        send_beacon(server_url, my_id, info);
        sleep(10); 
    }
    return 0;
}