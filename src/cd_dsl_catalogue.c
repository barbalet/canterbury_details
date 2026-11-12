#include "cd_dsl_catalogue.h"
#include <string.h>
#include "../build/content/CanterburyDefinitions.inc"

#define COUNT(array) (sizeof(array) / sizeof((array)[0]))

const char *cd_dsl_catalogue_identity(void) { return definition_identity; }
size_t cd_dsl_career_count(void) { return COUNT(career_families); }
size_t cd_dsl_role_count(void) { return COUNT(career_roles); }
size_t cd_dsl_advance_count(void) { return COUNT(career_transitions); }
size_t cd_dsl_resource_count(void) { return COUNT(resource_kinds); }
const CdDslResource *cd_dsl_resource_at(size_t index) {
  return index < COUNT(resource_kinds) ? &resource_kinds[index] : NULL;
}
size_t cd_dsl_recipe_count(void) { return COUNT(material_recipes); }

const CdDslCareer *cd_dsl_career(const char *id) {
  if (id)
    for (size_t i = 0; i < COUNT(career_families); ++i)
      if (strcmp(career_families[i].id, id) == 0)
        return &career_families[i];
  return NULL;
}

const CdDslRole *cd_dsl_role(const char *id) {
  if (id)
    for (size_t i = 0; i < COUNT(career_roles); ++i)
      if (strcmp(career_roles[i].id, id) == 0)
        return &career_roles[i];
  return NULL;
}

const CdDslAdvance *cd_dsl_advance(const char *id) {
  if (id)
    for (size_t i = 0; i < COUNT(career_transitions); ++i)
      if (strcmp(career_transitions[i].id, id) == 0)
        return &career_transitions[i];
  return NULL;
}

const CdDslResource *cd_dsl_resource(const char *id) {
  if (id)
    for (size_t i = 0; i < COUNT(resource_kinds); ++i)
      if (strcmp(resource_kinds[i].id, id) == 0)
        return &resource_kinds[i];
  return NULL;
}

const CdDslRecipe *cd_dsl_recipe(const char *id) {
  if (id)
    for (size_t i = 0; i < COUNT(material_recipes); ++i)
      if (strcmp(material_recipes[i].id, id) == 0)
        return &material_recipes[i];
  return NULL;
}

const CdDslRecipe *cd_dsl_recipe_at(size_t index) {
  return index < COUNT(material_recipes) ? &material_recipes[index] : NULL;
}

const CdDslRole *cd_dsl_role_at(size_t index) {
  return index < COUNT(career_roles) ? &career_roles[index] : NULL;
}
