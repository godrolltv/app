#ifndef PERKALIASES_H
#define PERKALIASES_H

#include <QMap>
#include <QString>

/**
 * Common perk abbreviations/aliases used by the Destiny 2 community
 * Maps shorthand -> full perk name
 * 
 * To add a new alias, simply add a new line:
 *   {"alias", "Full Perk Name"},
 */
static const QMap<QString, QString> PERK_ALIASES = {
    // ============================================
    // DAMAGE PERKS
    // ============================================
    {"kc", "Kill Clip"},
    {"mkc", "Multikill Clip"},
    {"despo", "Desperado"},
    {"ofa", "One for All"},
    {"swash", "Swashbuckler"},
    {"aj", "Adrenaline Junkie"},
    {"adrenaline", "Adrenaline Junkie"},
    {"fttc", "Fourth Time's the Charm"},
    {"ttc", "Fourth Time's the Charm"},
    {"vorpal", "Vorpal Weapon"},
    {"vw", "Vorpal Weapon"},
    {"fom", "Firing Line"},
    {"fl", "Firing Line"},
    {"bs", "Bait and Switch"},
    {"bns", "Bait and Switch"},
    
    // ============================================
    // RELOAD PERKS
    // ============================================
    {"ff", "Feeding Frenzy"},
    {"tt", "Triple Tap"},
    {"rr", "Rewind Rounds"},
    {"rewind", "Rewind Rounds"},
    {"cc", "Clown Cartridge"},
    {"clown", "Clown Cartridge"},
    
    // ============================================
    // UTILITY PERKS
    // ============================================
    {"os", "Opening Shot"},
    {"ss", "Snapshot Sights"},
    {"snapshot", "Snapshot Sights"},
    {"qd", "Quickdraw"},
    {"ep", "Explosive Payload"},
    {"tp", "Timed Payload"},
    {"timed", "Timed Payload"},
    {"demo", "Demolitionist"},
    {"destab", "Destabilizing Rounds"},
    {"dr", "Destabilizing Rounds"},
    {"destabilizing", "Destabilizing Rounds"},
    {"inc", "Incandescent"},
    {"volt", "Voltshot"},
    {"vs", "Voltshot"},
    {"chill", "Chill Clip"},
    {"hs", "Headstone"},
    {"df", "Dragonfly"},
    {"hatchling", "Hatchling"},
    
    // ============================================
    // PVP PERKS
    // ============================================
    {"rf", "Rangefinder"},
    {"eots", "Eye of the Storm"},
    {"eye", "Eye of the Storm"},
    {"zm", "Zen Moment"},
    {"zen", "Zen Moment"},
    {"mt", "Moving Target"},
    {"mtt", "Moving Target"},
    {"moving", "Moving Target"},
    {"slide", "Slideshot"},
    {"sw", "Slideways"},
    
    // ============================================
    // BARRELS (Column 1)
    // ============================================
    {"ab", "Arrowhead Brake"},
    {"arrowhead", "Arrowhead Brake"},
    {"hf", "Hammer-Forged Rifling"},
    {"hammer", "Hammer-Forged Rifling"},
    {"sb", "Smallbore"},
    {"fb", "Full Bore"},
    {"fullbore", "Full Bore"},
    {"cr", "Corkscrew Rifling"},
    {"corkscrew", "Corkscrew Rifling"},
    {"fluted", "Fluted Barrel"},
    {"pr", "Polygonal Rifling"},
    {"polygonal", "Polygonal Rifling"},
    {"eb", "Extended Barrel"},
    {"extbarrel", "Extended Barrel"},
    {"chambered", "Chambered Compensator"},
    
    // ============================================
    // MAGAZINE PERKS (Column 2)
    // ============================================
    {"tac", "Tactical Mag"},
    {"tacmag", "Tactical Mag"},
    {"tm", "Tactical Mag"},
    {"am", "Appended Mag"},
    {"appended", "Appended Mag"},
    {"em", "Extended Mag"},
    {"extmag", "Extended Mag"},
    {"fm", "Flared Magwell"},
    {"flared", "Flared Magwell"},
    {"rico", "Ricochet Rounds"},
    {"ricochet", "Ricochet Rounds"},
    {"hcr", "High-Caliber Rounds"},
    {"hir", "High-Impact Reserves"},
    {"alloy", "Alloy Magazine"},
    {"apr", "Armor-Piercing Rounds"},
    {"armor", "Armor-Piercing Rounds"},
    {"lm", "Light Mag"},
    {"light", "Light Mag"},
    {"acc", "Accurized Rounds"},
    {"ar", "Accurized Rounds"},
    {"accurized", "Accurized Rounds"},
    {"sr", "Steady Rounds"},
    {"steady", "Steady Rounds"},
    
    // ============================================
    // OTHER COMMON PERKS
    // ============================================
    {"sub", "Subsistence"},
    {"recon", "Reconstruction"},
    {"ea", "Envious Arsenal"},
    {"envious", "Envious Arsenal"},
    {"sfa", "Stats for All"},
    {"stats", "Stats for All"},
    {"aa", "Ambitious Assassin"},
    {"ambitious", "Ambitious Assassin"},
    {"pm", "Perpetual Motion"},
    {"perpetual", "Perpetual Motion"},
    {"td", "Threat Detector"},
    {"threat", "Threat Detector"},
    {"alh", "Auto-Loading Holster"},
    {"auto", "Auto-Loading Holster"},
    {"el", "Explosive Light"},
    {"explosive", "Explosive Light"},
};

#endif // PERKALIASES_H
