/*
 * MIT License
 *
 * Copyright (c) 2025 Soft Sprint Studios
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
#include "game_data.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include "gl_console.h"

static TGD_EntityDef g_entity_defs[MAX_TGD_ENTITIES];
static int g_num_entity_defs = 0;

static const char** g_brush_classnames = NULL;
static int g_num_brush_classnames = 0;

static const char** g_logic_classnames = NULL;
static int g_num_logic_classnames = 0;


static TGD_PropertyType string_to_prop_type(const char* type_str) {
    if (_stricmp(type_str, "string") == 0) return TGD_PROP_STRING;
    if (_stricmp(type_str, "integer") == 0) return TGD_PROP_INTEGER;
    if (_stricmp(type_str, "float") == 0) return TGD_PROP_FLOAT;
    if (_stricmp(type_str, "color") == 0) return TGD_PROP_COLOR;
    if (_stricmp(type_str, "checkbox") == 0) return TGD_PROP_CHECKBOX;
    if (_stricmp(type_str, "model") == 0) return TGD_PROP_MODEL;
    if (_stricmp(type_str, "sound") == 0) return TGD_PROP_SOUND;
    if (_stricmp(type_str, "particle") == 0) return TGD_PROP_PARTICLE;
    if (_stricmp(type_str, "choices") == 0) return TGD_PROP_CHOICES;
    if (_stricmp(type_str, "texture") == 0) return TGD_PROP_TEXTURE;
    return TGD_PROP_STRING;
}

void GameData_Init(const char* filepath) {
    FILE* file = fopen(filepath, "r");
    if (!file) {
        Console_Printf_Error("Could not open TGD file: %s", filepath);
        return;
    }

    char line[1024];
    TGD_EntityDef* current_def = NULL;

    while (fgets(line, sizeof(line), file)) {
        char* trimmed = trim(line);
        if (strlen(trimmed) == 0 || strncmp(trimmed, "//", 2) == 0) continue;

        if (strncmp(trimmed, "@SolidClass", 11) == 0 || strncmp(trimmed, "@PointClass", 11) == 0) {
            if (g_num_entity_defs >= MAX_TGD_ENTITIES) break;
            current_def = &g_entity_defs[g_num_entity_defs++];
            memset(current_def, 0, sizeof(TGD_EntityDef));
            if (trimmed[1] == 'S') {
                current_def->base_type = ENTITY_BRUSH;
                sscanf(trimmed, "@SolidClass = %s", current_def->classname);
            }
            else {
                current_def->base_type = ENTITY_LOGIC;
                sscanf(trimmed, "@PointClass = %s", current_def->classname);
            }
        }
        else if (current_def && strstr(trimmed, "input ")) {
            if (current_def->num_inputs < MAX_TGD_IOS) {
                TGD_IO* io = &current_def->inputs[current_def->num_inputs++];
                sscanf(trimmed, "input %63s \"%127[^\"]\"", io->name, io->description);
            }
        }
        else if (current_def && strstr(trimmed, "output ")) {
            if (current_def->num_outputs < MAX_TGD_IOS) {
                TGD_IO* io = &current_def->outputs[current_def->num_outputs++];
                sscanf(trimmed, "output %63s \"%127[^\"]\"", io->name, io->description);
            }
        }
        else if (current_def && (trimmed[0] != '[' && trimmed[0] != ']')) {
            if (current_def->num_properties < MAX_TGD_PROPERTIES) {
                TGD_Property* prop = &current_def->properties[current_def->num_properties];
                memset(prop, 0, sizeof(TGD_Property));

                char type_str[32];
                int n = sscanf(trimmed, "%63[^(](%31[^)]) : \"%127[^\"]\" = \"%127[^\"]\"", prop->key, type_str, prop->display_name, prop->default_value);
                if (n >= 3) {
                    prop->type = string_to_prop_type(type_str);
                    current_def->num_properties++;

                    if (prop->type == TGD_PROP_CHOICES) {
                        char next_line[256];
                        long pos = ftell(file);
                        if (fgets(next_line, sizeof(next_line), file) && trim(next_line)[0] == '[') {
                            while (fgets(next_line, sizeof(next_line), file) && trim(next_line)[0] != ']') {
                                TGD_Choice choice;
                                if (sscanf(trim(next_line), "%63s : \"%127[^\"]\"", choice.value, choice.display_name) == 2) {
                                    prop->num_choices++;
                                    prop->choices = (TGD_Choice*)realloc(prop->choices, prop->num_choices * sizeof(TGD_Choice));
                                    prop->choices[prop->num_choices - 1] = choice;
                                }
                            }
                        }
                        else {
                            fseek(file, pos, SEEK_SET);
                        }
                    }
                }
            }
        }
    }
    fclose(file);

    g_num_brush_classnames = 1;
    g_num_logic_classnames = 0;
    for (int i = 0; i < g_num_entity_defs; i++) {
        if (g_entity_defs[i].classname[0] == '_') continue;
        if (g_entity_defs[i].base_type == ENTITY_BRUSH) g_num_brush_classnames++;
        else if (g_entity_defs[i].base_type == ENTITY_LOGIC) g_num_logic_classnames++;
    }

    g_brush_classnames = (const char**)malloc(g_num_brush_classnames * sizeof(const char*));
    g_logic_classnames = (const char**)malloc(g_num_logic_classnames * sizeof(const char*));

    g_brush_classnames[0] = "(None)";
    int brush_idx = 1;
    int logic_idx = 0;
    for (int i = 0; i < g_num_entity_defs; i++) {
        if (g_entity_defs[i].classname[0] == '_') continue;
        if (g_entity_defs[i].base_type == ENTITY_BRUSH) {
            g_brush_classnames[brush_idx++] = g_entity_defs[i].classname;
        }
        else if (g_entity_defs[i].base_type == ENTITY_LOGIC) {
            g_logic_classnames[logic_idx++] = g_entity_defs[i].classname;
        }
    }

    Console_Printf("Loaded %d entity definitions from TGD.", g_num_entity_defs);
}

void GameData_Shutdown(void) {
    for (int i = 0; i < g_num_entity_defs; ++i) {
        for (int j = 0; j < g_entity_defs[i].num_properties; ++j) {
            if (g_entity_defs[i].properties[j].choices) {
                free(g_entity_defs[i].properties[j].choices);
            }
        }
    }
    if (g_brush_classnames) free(g_brush_classnames);
    if (g_logic_classnames) free(g_logic_classnames);
    g_num_entity_defs = 0;
    g_num_brush_classnames = 0;
    g_num_logic_classnames = 0;
}

const TGD_EntityDef* GameData_FindEntityDef(const char* classname) {
    if (!classname || classname[0] == '\0') return NULL;
    for (int i = 0; i < g_num_entity_defs; ++i) {
        if (_stricmp(g_entity_defs[i].classname, classname) == 0) {
            return &g_entity_defs[i];
        }
    }
    return NULL;
}

const char** GameData_GetBrushEntityClassnames(int* count) {
    *count = g_num_brush_classnames;
    return g_brush_classnames;
}

const char** GameData_GetLogicEntityClassnames(int* count) {
    *count = g_num_logic_classnames;
    return g_logic_classnames;
}