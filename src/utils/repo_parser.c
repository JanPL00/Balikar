//
// Created by placeja5 on 11/4/25.
//

#include "include/repo_parser.h"

// Function to load JSON from file
json_t* load_json(const char* filepath) {
    json_error_t error;
    json_t* root = json_load_file(filepath, 0, &error);
    if (!root) {
        fprintf(stderr, "Error loading JSON from %s: %s\n", filepath, error.text);
        return NULL;
    }
    return root;
}

// Function to generate package key: "name-version-build.tar.bz2"
char* generate_package_key(json_t* index) {
    const char* name = json_string_value(json_object_get(index, "name"));
    const char* version = json_string_value(json_object_get(index, "version"));
    const char* build = json_string_value(json_object_get(index, "build"));
    if (!name || !version || !build) {
        return NULL;
    }
    size_t len = strlen(name) + strlen(version) + strlen(build) + 12;  // + for -, .tar.bz2, null
    char* key = malloc(len);
    if (key) {
        snprintf(key, len, "%s-%s-%s.tar.bz2", name, version, build);
    }
    return key;
}

// Recursive function to traverse directory and collect index.json files
void collect_packages(const char* root_path, json_t* packages) {
    DIR* dir = opendir(root_path);
    if (!dir) {
        fprintf(stderr, "Error opening directory %s\n", root_path);
        return;
    }

    struct dirent* entry;
    while ((entry = readdir(dir)) != NULL) {
        if (entry->d_type == DT_DIR && strcmp(entry->d_name, ".") != 0 && strcmp(entry->d_name, "..") != 0) {
            char subpath[1024];
            snprintf(subpath, sizeof(subpath), "%s/%s", root_path, entry->d_name);
            collect_packages(subpath, packages);
        } else if (strcmp(entry->d_name, "index.json") == 0) {
            char filepath[1024];
            snprintf(filepath, sizeof(filepath), "%s/%s", root_path, entry->d_name);
            json_t* index = load_json(filepath);
            if (index) {
                // Add required fields if missing (defaults for compatibility)
                if (!json_object_get(index, "build_number")) {
                    json_object_set_nocheck(index, "build_number", json_integer(0));
                }
                if (!json_object_get(index, "license")) {
                    json_object_set_nocheck(index, "license", json_string("unknown"));
                }
                if (!json_object_get(index, "md5")) {
                    json_object_set_nocheck(index, "md5", json_string(""));  // Placeholder; compute if needed
                }
                if (!json_object_get(index, "sha256")) {
                    json_object_set_nocheck(index, "sha256", json_string(""));  // Placeholder
                }
                if (!json_object_get(index, "size")) {
                    json_object_set_nocheck(index, "size", json_integer(0));  // Placeholder
                }
                if (!json_object_get(index, "subdir")) {
                    json_object_set_nocheck(index, "subdir", json_string("noarch"));
                }
                if (!json_object_get(index, "timestamp")) {
                    time_t now = time(NULL);
                    json_object_set_nocheck(index, "timestamp", json_integer((json_int_t)now * 1000));  // ms
                }

                char* key = generate_package_key(index);
                if (key) {
                    json_object_set_nocheck(packages, key, index);
                    free(key);
                } else {
                    json_decref(index);
                }
            }
        }
    }
    closedir(dir);
}

// Main function to generate repodata.json
int generate_repodata(const char* repo_path) {
    json_t* repodata = json_object();
    if (!repodata) return 1;

    json_t* info = json_object();
    json_object_set_nocheck(info, "subdir", json_string("noarch"));  // Adjust as needed
    json_object_set_nocheck(info, "platform", json_string("linux"));
    json_object_set_nocheck(info, "arch", json_string("x86_64"));
    json_object_set_nocheck(repodata, "info", info);

    json_t* packages = json_object();
    collect_packages(repo_path, packages);
    json_object_set_nocheck(repodata, "packages", packages);

    // Optional: Add "packages.conda" if using .conda format; here assuming .tar.bz2 only
    json_t* packages_conda = json_object();
    json_object_set_nocheck(repodata, "packages.conda", packages_conda);

    // Write to repodata.json
    char output_path[1024];
    snprintf(output_path, sizeof(output_path), "%s/repodata.json", repo_path);
    if (json_dump_file(repodata, output_path, JSON_INDENT(4)) != 0) {
        fprintf(stderr, "Error writing repodata.json\n");
        json_decref(repodata);
        return 1;
    }

    json_decref(repodata);
    printf("repodata.json generated at %s\n", output_path);
    return 0;
}

int main(int argc, char** argv) {
    if (argc != 2) {
        fprintf(stderr, "Usage: %s repo\n", argv[0]);
        return 1;
    }
    return generate_repodata(argv[1]);
}