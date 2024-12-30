#ifndef SPOTIFYCLIENT_H
#define SPOTIFYCLIENT_H

#include <string>
#include <vector>
#include <curl/curl.h>
#include "/opt/homebrew/Cellar/nlohmann-json/3.11.3/include/nlohmann/json.hpp"

using json = nlohmann::json;

// Album class to store album information
class Album {
public:
    std::string name;
    std::string artist;
    std::string id;
    std::string release_date;
    std::string image_url;
    int rating;

    Album(const std::string& name, const std::string& artist, const std::string& id,
          const std::string& release_date, const std::string& image_url)
        : name(name), artist(artist), id(id), release_date(release_date), image_url(image_url), rating(0) {}
};

// Change the WriteCallback function to be inline
inline size_t WriteCallback(void* contents, size_t size, size_t nmemb, std::string* userp) {
    userp->append((char*)contents, size * nmemb);
    return size * nmemb;
}

class SpotifyClient {
private:
    std::string client_id;
    std::string client_secret;
    std::string access_token;

    // Make WriteCallback a static member function
    static size_t WriteCallback(void* contents, size_t size, size_t nmemb, std::string* userp) {
        userp->append(static_cast<char*>(contents), size * nmemb);
        return size * nmemb;
    }

    // Base64 encoding helper function can remain as a private member
    std::string base64_encode(const std::string& input) const {
        static const std::string base64_chars = 
            "ABCDEFGHIJKLMNOPQRSTUVWXYZabcdefghijklmnopqrstuvwxyz0123456789+/";
        
        std::string encoded;
        int val = 0, valb = -6;
        for (unsigned char c : input) {
            val = (val << 8) + c;
            valb += 8;
            while (valb >= 0) {
                encoded.push_back(base64_chars[(val >> valb) & 0x3F]);
                valb -= 6;
            }
        }
        if (valb > -6) encoded.push_back(base64_chars[((val << 8) >> (valb + 8)) & 0x3F]);
        while (encoded.size() % 4) encoded.push_back('=');
        return encoded;
    }

    bool authenticate() {
        CURL* curl = curl_easy_init();
        if (!curl) return false;

        // Prepare the authorization header
        std::string auth_str = client_id + ":" + client_secret;
        std::string auth_header = "Authorization: Basic " + base64_encode(auth_str);
        
        // Set up the POST request
        std::string response;
        struct curl_slist* headers = nullptr;
        headers = curl_slist_append(headers, auth_header.c_str());
        headers = curl_slist_append(headers, "Content-Type: application/x-www-form-urlencoded");

        curl_easy_setopt(curl, CURLOPT_URL, "https://accounts.spotify.com/api/token");
        curl_easy_setopt(curl, CURLOPT_HTTPHEADER, headers);
        curl_easy_setopt(curl, CURLOPT_POSTFIELDS, "grant_type=client_credentials");
        curl_easy_setopt(curl, CURLOPT_WRITEFUNCTION, WriteCallback);
        curl_easy_setopt(curl, CURLOPT_WRITEDATA, &response);

        CURLcode res = curl_easy_perform(curl);
        curl_slist_free_all(headers);
        curl_easy_cleanup(curl);

        if (res != CURLE_OK) return false;

        try {
            json j = json::parse(response);
            access_token = j.at("access_token").get<std::string>();
            return true;
        } catch (...) {
            return false;
        }
    }

public:
    SpotifyClient(const std::string& id, const std::string& secret) 
        : client_id(id), client_secret(secret) {
        if (!authenticate()) {
            throw std::runtime_error("Failed to authenticate with Spotify");
        }
    }

    struct SearchResult {
        std::vector<Album> albums;
        int total;
        bool hasMore;
        int nextOffset;
    };

    SearchResult searchAlbums(const std::string& query, int offset = 0) {
        SearchResult result;
        result.albums.clear();
        result.total = 0;
        result.hasMore = false;
        result.nextOffset = offset;

        CURL* curl = curl_easy_init();
        if (!curl) return result;

        char* encoded_query = curl_easy_escape(curl, query.c_str(), static_cast<int>(query.length()));
        std::string url = "https://api.spotify.com/v1/search?q=" + 
                         std::string(encoded_query) + 
                         "&type=album&limit=10&offset=" + 
                         std::to_string(offset);
        curl_free(encoded_query);

        // Set up the GET request
        std::string response;
        struct curl_slist* headers = nullptr;
        std::string auth_header = "Authorization: Bearer " + access_token;
        headers = curl_slist_append(headers, auth_header.c_str());

        curl_easy_setopt(curl, CURLOPT_URL, url.c_str());
        curl_easy_setopt(curl, CURLOPT_HTTPHEADER, headers);
        curl_easy_setopt(curl, CURLOPT_WRITEFUNCTION, WriteCallback);
        curl_easy_setopt(curl, CURLOPT_WRITEDATA, &response);

        CURLcode res = curl_easy_perform(curl);
        curl_slist_free_all(headers);
        curl_easy_cleanup(curl);

        if (res == CURLE_OK) {
            try {
                json albumsJson = json::parse(response).at("albums");
                json items = albumsJson.at("items");
                
                // Get total number of results
                result.total = albumsJson.at("total").get<int>();
                
                // Calculate if there are more results
                int limit = albumsJson.at("limit").get<int>();
                result.nextOffset = offset + limit;
                result.hasMore = result.nextOffset < result.total;

                for (const auto& album : items) {
                    std::string image_url = album.at("images")[0].at("url").get<std::string>();
                    std::string artist_name = album.at("artists")[0].at("name").get<std::string>();
                    
                    result.albums.emplace_back(
                        album.at("name").get<std::string>(),
                        artist_name,
                        album.at("id").get<std::string>(),
                        album.at("release_date").get<std::string>(),
                        image_url
                    );
                }
            } catch (...) {
                // Handle parsing errors
            }
        }

        return result;
    }
};

#endif // SPOTIFYCLIENT_H 