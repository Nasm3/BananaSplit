//! Detection of the optional virtual cable drivers on Windows: Splitter
//! (BananaSplit's own) and VB-Audio's.
use super::route;
use super::wasapi::Enumerator;
use bananasplit::config::Config;

pub const VBCABLE_PAGE: &str = "https://vb-audio.com/Cable/";

pub fn installed_cables(en: &Enumerator) -> Vec<String> {
    let mut v = Vec::new();
    for capture in [false, true] {
        if let Ok(devs) = en.devices(capture) {
            v.extend(devs.into_iter().map(|d| d.info.name).filter(|n| route::is_cable(n)));
        }
    }
    v
}

/// Report missing drivers (a note for the status line). True if none is
/// installed.
pub fn first_run(en: &Enumerator, _cfg: &mut Config, _headless: bool, notes: &mut Vec<String>) -> bool {
    let missing = installed_cables(en).is_empty();
    if missing {
        notes.push(super::chanmenu::no_cables_text());
    }
    missing
}
