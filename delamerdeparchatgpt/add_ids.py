import json

# Charge le fichier JSON
with open('/home/thibault/Documents/important/TRAVAIL/sites/applisMusique/HOMEBREWS/ds/mazhoot/poly3/root_drum_machine_version_github_propre/root_drum_machine_ds/data.json', 'r') as f:
    data = json.load(f)

# Initialise le compteur pour les identifiants
id_counter = 0

# Parcours les SFX et assigne des identifiants uniques
for sfx in data['sfx']:
    sfx['id'] = id_counter
    id_counter += 1

# Parcours les mods et assigne des identifiants uniques
mod_ids = []
for mod in data['mods']:
    # Combine les identifiants des SFX et des mods déjà existants pour générer des identifiants uniques
    unique_ids = mod_ids + [sfx['id'] for sfx in data['sfx']] 
    # Trouve le premier identifiant disponible
    while id_counter in unique_ids:
        id_counter += 1
    # Assigner l'identifiant unique au mod
    mod['id'] = id_counter
    # Ajouter l'identifiant unique à la liste des identifiants de mods
    mod_ids.append(id_counter)

# Enregistre les modifications dans le fichier JSON
with open('/home/thibault/Documents/important/TRAVAIL/sites/applisMusique/HOMEBREWS/ds/mazhoot/poly3/root_drum_machine_version_github_propre/root_drum_machine_ds/data.json', 'w') as f:
    json.dump(data, f)
