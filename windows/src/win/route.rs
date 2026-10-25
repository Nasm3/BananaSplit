//! Resolving `connect =` entries to Windows audio devices (pure logic, no
//! Windows calls, so it is unit-tested on every platform).
//!
//! On Windows a `connect` value is either `@physical` (the default device,
//! skipping virtual cables so the mixer never feeds itself) or a
//! case-insensitive fragment of a device's name as shown in the Windows
//! sound settings, e.g. `CABLE Output` or `Headset Earphone`.

use bananasplit::config::{DEFAULT_DEVICE, PHYSICAL};

#[derive(Debug, Clone, PartialEq, Eq)]
pub struct DevInfo {
    /// Friendly name, e.g. "CABLE Output (VB-Audio Virtual Cable)".
    pub name: String,
    /// Endpoint ID string (stable across reboots).
    pub id: String,
}

/// Virtual cables and other software devices. Never picked for `@physical`:
/// if the Windows default output is "CABLE Input" (so all desktop audio
/// goes through the mixer), A1 must still go to real speakers.
pub fn is_virtual(name: &str) -> bool {
    let n = name.to_ascii_lowercase();
    n.contains("vb-audio")
        || n.starts_with("cable")
        || n.contains("voicemeeter")
        || n.contains("virtual")
        || n.contains("hi-fi cable")
        || is_splitter(name)
}

/// The device name of BananaSplit's own cables, as Windows shows it after
/// each endpoint's name: "Splitter 3 (Splitter Virtual Cables)".
pub const SPLITTER_DEVICE: &str = "Splitter Virtual Cables";

/// Where the Splitter driver is published.
pub const SPLITTER_PAGE: &str = "https://github.com/LudaLunizz/BananaSplit/releases?q=splitter";

/// The two halves of a Splitter cable.
#[derive(Debug, Clone, Copy, PartialEq, Eq)]
pub enum SplitterEnd {
    /// "Splitter N": the playback device apps play into.
    Input,
    /// "Splitter N Output": the recording device strip N records.
    Output,
}

/// Endpoints of the Splitter driver (any of its six cables).
pub fn is_splitter(name: &str) -> bool {
    name.to_ascii_lowercase()
        .ends_with(&format!("({})", SPLITTER_DEVICE.to_ascii_lowercase()))
}

/// Which Splitter cable (1..=6) and which half an endpoint is, from its
/// name: "Splitter 2 (Splitter Virtual Cables)" is cable 2's input,
/// "Splitter 2 Output (Splitter Virtual Cables)" its output. An endpoint
/// renamed in the Windows sound settings is still a Splitter endpoint
/// (`is_splitter`) but no longer says which cable it is: `None`.
pub fn splitter_cable(name: &str) -> Option<(usize, SplitterEnd)> {
    if !is_splitter(name) {
        return None;
    }
    let mut endpoint = short_name(name).trim().to_ascii_lowercase();
    // Windows numbers endpoints whose names clash, e.g. after a reinstall
    // left an old device behind: "2- Splitter 1 (Splitter Virtual Cables)".
    if let Some(dash) = endpoint.find("- ") {
        if dash > 0 && endpoint[..dash].bytes().all(|b| b.is_ascii_digit()) {
            endpoint = endpoint[dash + 2..].to_string();
        }
    }
    let rest = endpoint.strip_prefix("splitter ")?;
    let (num, end) = match rest.strip_suffix(" output") {
        Some(num) => (num, SplitterEnd::Output),
        None => (rest, SplitterEnd::Input),
    };
    if num.is_empty() || !num.bytes().all(|b| b.is_ascii_digit()) || num.len() > 2 {
        return None;
    }
    let n: usize = num.parse().ok()?;
    (1..=6).contains(&n).then_some((n, end))
}

/// Any virtual cable BananaSplit records or plays into: VB-Audio's or its
/// own (Splitter).
pub fn is_cable(name: &str) -> bool {
    is_vb_cable(name) || is_splitter(name)
}

/// True for VB-Audio cable endpoints (what the install prompt looks for).
pub fn is_vb_cable(name: &str) -> bool {
    let n = name.to_ascii_lowercase();
    n.contains("vb-audio") && (n.starts_with("cable") || n.contains("cable"))
}

/// A pattern that names a virtual cable. Missing cables are reported as
/// "not installed" rather than as errors (most people only have some).
pub fn names_cable(pattern: &str) -> bool {
    let p = pattern.trim().to_ascii_lowercase();
    p.contains("cable") || p.starts_with("splitter")
}

/// The Windows default device, when it is a virtual cable (VB-Audio's or
/// Splitter): `@default`
/// does not use it. BananaSplit records the cables and plays into them, so
/// following such a default would feed the mixer into itself. That happens
/// with two everyday setups: the default output set to "CABLE Input" (all
/// desktop sound through the mixer: A1 would play into the cable a strip
/// records), and the default input set to "CABLE Output" (the virtual mic
/// for every app: the Mic strip would record what B1 plays into it).
pub fn default_cable<'a>(devs: &'a [DevInfo], default_id: Option<&str>) -> Option<&'a DevInfo> {
    default_id.and_then(|id| devs.iter().find(|d| d.id == id)).filter(|d| is_cable(&d.name))
}

/// Find the device for one `connect` entry.
pub fn resolve<'a>(
    pattern: &str,
    devs: &'a [DevInfo],
    default_id: Option<&str>,
) -> Option<&'a DevInfo> {
    if pattern == DEFAULT_DEVICE {
        // Whatever Windows uses by default, unless that is a virtual cable
        // (see `default_cable`): then the real device `@physical` picks.
        if default_cable(devs, default_id).is_some() {
            return resolve(PHYSICAL, devs, default_id);
        }
        return default_id
            .and_then(|id| devs.iter().find(|d| d.id == id))
            .or_else(|| devs.iter().find(|d| !is_cable(&d.name)));
    }
    if pattern == PHYSICAL {
        let default = default_id.and_then(|id| devs.iter().find(|d| d.id == id));
        return match default {
            Some(d) if !is_virtual(&d.name) => Some(d),
            _ => devs.iter().find(|d| !is_virtual(&d.name)),
        };
    }
    let p = pattern.trim().to_lowercase();
    if p.is_empty() {
        return None;
    }
    let lower: Vec<String> = devs.iter().map(|d| d.name.to_lowercase()).collect();
    // exact name, then prefix, then anywhere in the name
    if let Some(i) = lower.iter().position(|n| *n == p) {
        return Some(&devs[i]);
    }
    if let Some(i) = lower.iter().position(|n| n.starts_with(&p)) {
        return Some(&devs[i]);
    }
    lower.iter().position(|n| n.contains(&p)).map(|i| &devs[i])
}

/// Short name for the UI: "CABLE Output (VB-Audio Virtual Cable)" -> "CABLE Output".
pub fn short_name(name: &str) -> &str {
    match name.find(" (") {
        Some(i) if i > 0 => &name[..i],
        _ => name,
    }
}

#[cfg(test)]
mod tests {
    use super::*;

    fn devs(names: &[&str]) -> Vec<DevInfo> {
        names
            .iter()
            .enumerate()
            .map(|(i, n)| DevInfo {
                name: n.to_string(),
                id: format!("id{i}"),
            })
            .collect()
    }

    #[test]
    fn default_never_follows_windows_into_a_cable() {
        let outs = devs(&[
            "CABLE Input (VB-Audio Virtual Cable)",
            "Speakers (Realtek(R) Audio)",
            "Headphones (USB Headset)",
        ]);
        // default output "CABLE Input" (desktop sound through the mixer):
        // A1 plays on the speakers, not into the cable a strip records
        assert_eq!(resolve(DEFAULT_DEVICE, &outs, Some("id0")).unwrap().id, "id1");
        assert_eq!(default_cable(&outs, Some("id0")).unwrap().id, "id0");
        // a real default is followed as before
        assert_eq!(resolve(DEFAULT_DEVICE, &outs, Some("id2")).unwrap().id, "id2");
        assert!(default_cable(&outs, Some("id2")).is_none());
        // no default known: not a cable either
        assert_eq!(resolve(DEFAULT_DEVICE, &outs, None).unwrap().id, "id1");
        // default input "CABLE Output" (the virtual mic for every app): the
        // Mic strip records the real microphone, not what B1 plays into it
        let ins = devs(&["CABLE Output (VB-Audio Virtual Cable)", "Microphone (Yeti Stereo Microphone)"]);
        assert_eq!(resolve(DEFAULT_DEVICE, &ins, Some("id0")).unwrap().id, "id1");
        // only cables: nothing, rather than a loop
        assert!(resolve(DEFAULT_DEVICE, &devs(&["CABLE Input (VB-Audio Virtual Cable)"]), Some("id0")).is_none());
    }

    #[test]
    fn physical_skips_virtual_default() {
        let d = devs(&[
            "CABLE Input (VB-Audio Virtual Cable)",
            "Speakers (Realtek(R) Audio)",
            "Headphones (USB Headset)",
        ]);
        // default is the cable -> pick the first real device instead
        assert_eq!(resolve(PHYSICAL, &d, Some("id0")).unwrap().id, "id1");
        // default is a real device -> use it
        assert_eq!(resolve(PHYSICAL, &d, Some("id2")).unwrap().id, "id2");
        // no default at all
        assert_eq!(resolve(PHYSICAL, &d, None).unwrap().id, "id1");
        assert!(
            resolve(
                PHYSICAL,
                &devs(&["CABLE Input (VB-Audio Virtual Cable)"]),
                Some("id0")
            )
            .is_none()
        );
    }

    #[test]
    fn cable_names_do_not_collide() {
        let d = devs(&[
            "CABLE-A Output (VB-Audio Cable A)",
            "CABLE Output (VB-Audio Virtual Cable)",
            "CABLE-B Output (VB-Audio Cable B)",
        ]);
        assert_eq!(resolve("CABLE Output", &d, None).unwrap().id, "id1");
        assert_eq!(resolve("cable-b output", &d, None).unwrap().id, "id2");
        assert_eq!(resolve("Cable A", &d, None).unwrap().id, "id0");
        assert!(resolve("CABLE-C Output", &d, None).is_none());
        assert!(resolve("", &d, None).is_none());
    }

    #[test]
    fn exact_beats_prefix_beats_substring() {
        let d = devs(&["My Mic Pro", "Mic", "USB Mic"]);
        assert_eq!(resolve("mic", &d, None).unwrap().id, "id1");
        assert_eq!(resolve("my mic", &d, None).unwrap().id, "id0");
        assert_eq!(resolve("usb", &d, None).unwrap().id, "id2");
    }

    #[test]
    fn splitter_cables_are_numbered() {
        assert_eq!(
            splitter_cable("Splitter 1 (Splitter Virtual Cables)"),
            Some((1, SplitterEnd::Input))
        );
        assert_eq!(
            splitter_cable("Splitter 6 Output (Splitter Virtual Cables)"),
            Some((6, SplitterEnd::Output))
        );
        // case doesn't matter, the number does
        assert_eq!(
            splitter_cable("SPLITTER 3 output (splitter virtual cables)"),
            Some((3, SplitterEnd::Output))
        );
        assert_eq!(splitter_cable("Splitter 7 (Splitter Virtual Cables)"), None);
        assert_eq!(splitter_cable("Splitter 0 Output (Splitter Virtual Cables)"), None);
        assert_eq!(splitter_cable("Splitter 1x (Splitter Virtual Cables)"), None);
        // Windows' prefix for clashing names doesn't hide the number
        assert_eq!(
            splitter_cable("2- Splitter 5 Output (Splitter Virtual Cables)"),
            Some((5, SplitterEnd::Output))
        );
        // renamed in the sound settings: still a Splitter endpoint, no number
        assert!(is_splitter("Discord out (Splitter Virtual Cables)"));
        assert_eq!(splitter_cable("Discord out (Splitter Virtual Cables)"), None);
        // someone else's device that happens to be called Splitter
        assert!(!is_splitter("Splitter 1 (USB Audio Splitter)"));
        assert_eq!(splitter_cable("Splitter 1 (USB Audio Splitter)"), None);
        // Splitter endpoints are virtual cables
        assert!(is_virtual("Splitter 2 (Splitter Virtual Cables)"));
        assert!(is_cable("Splitter 2 Output (Splitter Virtual Cables)"));
        assert!(names_cable("Splitter 4"));
        assert_eq!(short_name("Splitter 4 Output (Splitter Virtual Cables)"), "Splitter 4 Output");
    }

    #[test]
    fn default_never_follows_windows_into_splitter() {
        // default output "Splitter 1" (all desktop sound into strip 1): A1
        // plays on the speakers, not back into the cable strip 1 records
        let outs = devs(&["Splitter 1 (Splitter Virtual Cables)", "Speakers (Realtek(R) Audio)"]);
        assert_eq!(resolve(DEFAULT_DEVICE, &outs, Some("id0")).unwrap().id, "id1");
        assert_eq!(default_cable(&outs, Some("id0")).unwrap().id, "id0");
        assert_eq!(resolve(PHYSICAL, &outs, Some("id0")).unwrap().id, "id1");
        // default input a Splitter output: the Mic strip records the mic
        let ins = devs(&["Splitter 1 Output (Splitter Virtual Cables)", "Microphone (USB Mic)"]);
        assert_eq!(resolve(DEFAULT_DEVICE, &ins, Some("id0")).unwrap().id, "id1");
        // a numbered pattern finds exactly its cable
        let all = devs(&[
            "Splitter 1 Output (Splitter Virtual Cables)",
            "Splitter 2 Output (Splitter Virtual Cables)",
        ]);
        assert_eq!(resolve("Splitter 2 Output", &all, None).unwrap().id, "id1");
    }

    #[test]
    fn classification() {
        assert!(is_vb_cable("CABLE Output (VB-Audio Virtual Cable)"));
        assert!(is_vb_cable("CABLE-C Input (VB-Audio Cable C)"));
        assert!(!is_vb_cable("Speakers (Realtek(R) Audio)"));
        assert!(is_virtual("VoiceMeeter Input (VB-Audio VoiceMeeter VAIO)"));
        assert!(!is_virtual("Microphone (HD Pro Webcam C920)"));
        assert!(names_cable("CABLE-D Output"));
        assert_eq!(
            short_name("CABLE Output (VB-Audio Virtual Cable)"),
            "CABLE Output"
        );
        assert_eq!(short_name("Mic"), "Mic");
    }
}

