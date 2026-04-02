/*
 * MIT License
 *
 * Copyright (c) 2025-2026 Soft Sprint Studios
 *
 * Permission is hereby granted, free of charge, to any person obtaining a copy
 * of this software and associated documentation files (the "Software"), to deal
 * in the Software without restriction, including without limitation the rights
 * to use, copy, modify, merge, publish, distribute, sublicense, and/or sell
 * copies of the Software, and to permit persons to whom the Software is
 * furnished to do so, subject to the following conditions:
 *
 * The above copyright notice and this permission notice shall be included in all
 * copies or substantial portions of the Software.
 *
 * THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS OR
 * IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY,
 * FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT. IN NO EVENT SHALL THE
 * AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER
 * LIABILITY, WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING FROM,
 * OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER DEALINGS IN THE
 * SOFTWARE.
 */
#include <iostream>
#include <fstream>
#include <string>
#include <vector>
#include <set>
#include <filesystem>
#include <algorithm>
#include <map>
#include <sstream>
#include <iomanip>

namespace fs = std::filesystem;

struct MaterialInfo
{
    string diffusePath;
    string normalPath;
    string rmaPath;
    string heightPath;
};

struct WaterInfo
{
    string normalPath;
    string dudvPath;
    string flowmapPath;
};

string trim(const string& str)
{
    size_t first = str.find_first_not_of(" \t\n\r");
    if (first == string::npos) return str;

    size_t last = str.find_last_not_of(" \t\n\r");
    return str.substr(first, last - first + 1);
}

bool string_ends_with(const string& str, const string& suffix)
{
    return str.length() >= suffix.length() &&
        str.compare(str.length() - suffix.length(), suffix.length(), suffix) == 0;
}

set<string> get_all_files_in(const fs::path& dir, const vector<string>& extensions)
{
    set<string> files;

    if (!fs::exists(dir) || !fs::is_directory(dir))
        return files;

    for (const auto& entry : fs::recursive_directory_iterator(dir))
    {
        if (!entry.is_regular_file())
            continue;

        string ext = entry.path().extension().string();
        transform(ext.begin(), ext.end(), ext.begin(),
            [](unsigned char c) { return tolower(c); });

        for (const auto& valid_ext : extensions)
        {
            if (ext == valid_ext)
            {
                string rel = fs::relative(entry.path(), fs::current_path()).string();
                replace(rel.begin(), rel.end(), '\\', '/');
                files.insert(rel);
                break;
            }
        }
    }

    return files;
}

map<string, MaterialInfo> parse_materials_def(const string& path)
{
    map<string, MaterialInfo> materials;
    ifstream file(path);

    string line;
    string current_name;
    MaterialInfo current_mat;
    bool in_block = false;

    while (getline(file, line))
    {
        line = trim(line);

        if (line.empty() || line[0] == '/' || line[0] == '#')
            continue;

        if (line[0] == '"')
        {
            current_name = line.substr(1, line.find('"', 1) - 1);
            current_mat = {};
        }
        else if (line == "{")
        {
            in_block = true;
        }
        else if (line == "}")
        {
            materials[current_name] = current_mat;
            in_block = false;
        }
        else if (in_block)
        {
            stringstream ss(line);
            string key, eq, val;

            ss >> key >> eq >> quoted(val);

            if (key == "diffuse") current_mat.diffusePath = val;
            else if (key == "normal") current_mat.normalPath = val;
            else if (key == "arm") current_mat.rmaPath = val;
            else if (key == "height") current_mat.heightPath = val;
        }
    }

    return materials;
}

map<string, WaterInfo> parse_waters_def(const string& path)
{
    map<string, WaterInfo> waters;
    ifstream file(path);

    string line;
    string current_name;
    WaterInfo current_water;
    bool in_block = false;

    while (getline(file, line))
    {
        line = trim(line);

        if (line.empty() || line[0] == '/' || line[0] == '#')
            continue;

        if (line[0] == '"')
        {
            current_name = line.substr(1, line.find('"', 1) - 1);
            current_water = {};
        }
        else if (line == "{")
        {
            in_block = true;
        }
        else if (line == "}")
        {
            waters[current_name] = current_water;
            in_block = false;
        }
        else if (in_block)
        {
            stringstream ss(line);
            string key, eq, val;

            ss >> key >> eq >> quoted(val);

            if (key == "normal") current_water.normalPath = val;
            else if (key == "dudv") current_water.dudvPath = val;
            else if (key == "flowmap") current_water.flowmapPath = val;
        }
    }

    return waters;
}

void parse_map_file(const fs::path& path, set<string>& used_files, set<string>& used_mats, set<string>& used_waters)
{
    ifstream file(path);
    if (!file.is_open())
        return;

    string line;
    bool in_props = false;

    while (getline(file, line))
    {
        line = trim(line);
        if (line.empty())
            continue;

        stringstream ss(line);
        string key;
        ss >> key;

        if (key == "properties")
        {
            in_props = true;
            continue;
        }

        if (key == "}")
        {
            in_props = false;
            continue;
        }

        if (in_props)
        {
            string p_key, p_val;
            stringstream pss(line);

            if (pss >> quoted(p_key) >> quoted(p_val))
            {
                if (p_key == "water_def")
                    used_waters.insert(p_val);
                else if (p_key == "normal_map" ||
                    p_key == "texture" ||
                    p_key == "material")
                    used_mats.insert(p_val);
            }
            continue;
        }

        if (key == "f")
        {
            vector<string> tokens;
            string tok;

            while (ss >> tok)
                tokens.push_back(tok);

            for (size_t i = 1; i < tokens.size() && i <= 4; i++)
            {
                string s = tokens[i];

                if (s.empty() ||
                    s == "null" ||
                    s == "___MISSING___" ||
                    s == "nodraw")
                    continue;

                if (s.front() == '"')
                    s = s.substr(1, s.length() - 2);

                used_mats.insert(s);
            }
        }
        else if (key == "gltf_model")
        {
            string m;
            ss >> quoted(m);
            used_files.insert(m);
        }
        else if (key == "decal" || key == "env_overlay" || key == "sprite")
        {
            string m;
            ss >> quoted(m);
            used_mats.insert(m);
        }
        else if (key == "skybox")
        {
            int i;
            string n;

            ss >> i >> quoted(n);

            const char* s[] = {
                "_px.png","_nx.png","_py.png",
                "_ny.png","_pz.png","_nz.png"
            };

            for (auto x : s)
                used_files.insert("skybox/" + n + x);
        }
        else if (key == "sound_entity")
        {
            string n, p;
            ss >> quoted(n) >> quoted(p);
            used_files.insert(p);
        }
        else if (key == "particle_emitter")
        {
            string p, n;
            ss >> quoted(p) >> quoted(n);
            used_files.insert(p);
        }
        else if (key == "parallax_room")
        {
            string b, n;
            ss >> quoted(b) >> quoted(n);

            const char* s[] = {
                "_px.png","_nx.png","_py.png",
                "_ny.png","_pz.png","_nz.png"
            };

            for (auto x : s)
                used_files.insert(b + x);
        }
    }
}

void rewrite_materials_def(const string& path, const map<string, MaterialInfo>& all, const set<string>& used)
{
    ifstream in(path);
    ofstream out("materials_temp.def");

    string line;
    string current_name;
    vector<string> block;
    bool in_block = false;

    while (getline(in, line))
    {
        string t = trim(line);

        if (!in_block && !t.empty() && t[0] == '"')
        {
            current_name = t.substr(1, t.find('"', 1) - 1);
            block = { line };
            in_block = true;
        }
        else if (in_block)
        {
            block.push_back(line);

            if (t == "}")
            {
                if (used.count(current_name))
                {
                    for (auto& l : block)
                        out << l << "\n";
                }

                in_block = false;
            }
        }
        else
        {
            out << line << "\n";
        }
    }

    in.close();
    out.close();

    fs::remove(path);
    fs::rename("materials_temp.def", path);
}

int main(int argc, char* argv[])
{
    bool del = (argc > 1 && string(argv[1]) == "--delete");

    ofstream log("cleanup.log");

    if (!del)
    {
        cout << "Run with --delete to actually remove unused files.\n";
        log << "Dry run. Use --delete to remove unused files.\n";
    }

    string tex_dir = "textures/";
    string mod_dir = "models/";
    string snd_dir = "sounds/";
    string part_dir = "particles/";
    string med_dir = "media/";
    string sky_dir = "skybox/";

    auto all_mats = parse_materials_def("materials.def");
    auto all_waters = parse_waters_def("waters.def");

    set<string> used_assets;
    set<string> used_mat_names;
    set<string> used_water_defs;

    if (fs::exists("maps/"))
    {
        for (auto& e : fs::recursive_directory_iterator("maps/"))
        {
            if (e.path().extension() == ".map" ||
                e.path().extension() == ".sav")
            {
                parse_map_file(
                    e.path(),
                    used_assets,
                    used_mat_names,
                    used_water_defs);
            }
        }
    }

    for (auto& m : used_mat_names)
    {
        if (all_mats.count(m))
        {
            auto& i = all_mats[m];

            if (!i.diffusePath.empty()) used_assets.insert(tex_dir + i.diffusePath);
            if (!i.normalPath.empty())  used_assets.insert(tex_dir + i.normalPath);
            if (!i.rmaPath.empty())     used_assets.insert(tex_dir + i.rmaPath);
            if (!i.heightPath.empty())  used_assets.insert(tex_dir + i.heightPath);
        }
    }

    for (auto& w : used_water_defs)
    {
        if (all_waters.count(w))
        {
            auto& i = all_waters[w];

            if (!i.normalPath.empty()) used_assets.insert(tex_dir + i.normalPath);
            if (!i.dudvPath.empty())   used_assets.insert(tex_dir + i.dudvPath);
            if (!i.flowmapPath.empty())used_assets.insert(tex_dir + i.flowmapPath);
        }
    }

    used_assets.insert(med_dir + "cursor.png");
    used_assets.insert(med_dir + "menu.mpg");
    used_assets.insert(snd_dir + "flashlight01.wav");
    used_assets.insert(snd_dir + "footstep.wav");
    used_assets.insert(tex_dir + "brdf_lut.png");
    used_assets.insert(tex_dir + "clouds.png");
    used_assets.insert(tex_dir + "dev01.png");
    used_assets.insert(tex_dir + "dev01_normal.png");
    used_assets.insert(tex_dir + "dev02.png");
    used_assets.insert(tex_dir + "dev02_normal.png");
    used_assets.insert(mod_dir + "error.glb");

    auto disk_tex = get_all_files_in(tex_dir, { ".png",".jpg",".tga" });
    auto disk_mod = get_all_files_in(mod_dir, { ".gltf",".glb" });
    auto disk_snd = get_all_files_in(snd_dir, { ".wav",".mp3" });
    auto disk_sky = get_all_files_in(sky_dir, { ".png" });

    vector<fs::path> unused;

    auto check_unused = [&](set<string>& disk)
        {
            for (auto& f : disk)
            {
                if (!used_assets.count(f))
                    unused.push_back(f);
            }
        };

    check_unused(disk_tex);
    check_unused(disk_mod);
    check_unused(disk_snd);
    check_unused(disk_sky);

    for (auto& p : unused)
    {
        cout << "Unused: " << p.string() << "\n";
        log << "Unused: " << p.string() << "\n";

        if (del)
            fs::remove(p);
    }

    if (del)
    {
        rewrite_materials_def("materials.def", all_mats, used_mat_names);
        log << "Deleted unused assets.\n";
    }

    log.close();
    return 0;
}