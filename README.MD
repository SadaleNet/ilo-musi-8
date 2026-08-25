**(English: Scroll down for English | toki ike Inli li lon anpa pi toki pona)**

# ilo musi 8 - ilo musi pi nasin CHIP-8

kepeken ilo musi 8 (ilo musi ali) la sina ken musi e musi CHIP-8. ilo musi 8 la:

* nasin CHIP-8 ni li ken:
	* nasin CHIP-8 (VIP) li ken
	* nasin SuperCHIP li ken
	* nasin XO-CHIP li ken lili. ni li ken ala: toki `plane n` (kule) en ma sona >= 4096. ali ante li ken
* tenpo pali: 5400 ips (90 instructions/frame)
* nena: nena CHIP-8 ali li lon (0-9, A-F). nena namako X kin li lon
* suli poki: 135x70x24mm
* wawa: poki wawa AAx2 anu linja wawa "USB MicroB"
* sona musi: kepeken lipu sona "TF Card"
	* nasin FAT16 en nasin FAT32 taso (o kepeken ala nasin exFAT!)
	* [nasin nimi 8.3]([8.3 filename](https://en.wikipedia.org/wiki/8.3_filename)) taso
	* sina kin li ken pana e sona sin tawa ilo musi 8 kepeken lipu sona "TF Card"
* musi open li lon. sina pana e wawa tawa ilo musi 8 sina la sina ken lukin e sitelen pi wile sina
* toki mute li lon: toki pona (sitelen Lasin en sitelen pona) en [sitelen Sate](https://sadale.net/tok/sitelen-sate) en toki ike Inli
* sona ali ona li lon. jan ali li ken lukin e ona li ken ante e ona li ken pali e ona
* jan li ken pali e ona mute kepeken mani lili kepeken tenpo lili (sina pali e ona mute "20" la sina o pana e mani "$12x20" o kepeken tenpo "30minutesx20")
* jan li ken ante e lukin ona (jan li ken ante e musi open. jan li ken ante e sitelen lon selo.)

## sona seme li lon?

```
assembly/        # lon ma ni la sina kama sona e nasin pali pi ilo musi 8
bootloader/      # kepeken sona ni la sina ken pana e sona sin tawa ilo musi 8. ona li kama sona tan ILOMUSI8.BIN lon lipu sona
docs-assets/     # sitelen pi lipu README.MD
enclosure/       # sitelen pi lupa selo
firmware/        # kepeken sona ni la ilo musi 8 li pali. sina ken musi kepeken ilo musi 8 tan ni: sona ni li lon
kicad/           # lipu kiwen sona. ilo musi 8 li jo e lipu kiwen. jan li pana e sona tawa ona la ilo musi 8 li ken pali
label/           # sitelen selo en sitelen poki
manual/          # jan li lukin e ona la jan ni li kama sona e ilo musi 8 li ken musi kepeken ona
```

## o esun e ilo ni!

sina ken esun e ona lon [esun Sate](https://esun.sadale.net/)!

mi jo e mani namako la mi wile pana e ilo musi 8 tawa jan pi mani ala. :)

## o pali e ilo ni!

o lukin e lipu ni: "[assembly/](./assembly/)"

## ken

lipu anpa ni li mi ala. sina wile lukin e ken ona la o lukin e nimi insa ona:

* ./bootloader/linker_script
* ./bootloader/pff3a
* ./firmware/fatfs

mi pali e lipu ante ali. ken ona li ken "BSD 2-clause".

# ilo musi 8 - Handheld CHIP-8 Game Console

ilo musi 8 is a production-ready handheld CHIP-8 game console. Feature list:

* CHIP-8 Compatibility:
	* CHIP-8 (VIP): Full support
	* SuperCHIP: Full support
	* XO-CHIP: Partial support. No support for `plane n` (color mode) and memory beyond 4K. Everything else (e.g. audio) is fully supported
* Performance: 5400 ips (90 instructions/frame)
* Buttons: Full CHIP-8 button support (0-9, A-F), with an extra X key for escaping
* Dimensions: 135x70x24mm
* Power: AAx2 or USB MicroB
* Game Storage: TF card
	* Supports FAT16 and FAT32 (No support for exFAT)
	* Supports [8.3 filename](https://en.wikipedia.org/wiki/8.3_filename) only
	* Firmware update is supported via the card
* Bootrom support. Can be used for showing boot screen upon powering on of the device
* Multi-language UI: English, [Toki Pona](https://en.wikipedia.org/wiki/Toki_Pona) and [Sadalian Script](https://sadale.net/en/sadalian)
* Fully open source, including electronics, enclosure and firmware, with full BOM and assembly instruction.
* Optimized for small scale production in terms of material cost and box build assembly time (At batch of 20, it costs $12 and 30 minutes of assembly time per unit)
* Optimized for white-labeling (custom boot screen via the bootrom, custom branding sticker on enclosure, custom cardboard label, etc.)

## Repo Structure

```
assembly/        # Assembly instruction for you to build your own unit
bootloader/      # Bootloader for flashing the file "ILOMUSI8.BIN" on TF card as firmware
cardboard_label/ # Label slapped onto the packaging cardboard
docs-assets/     # Images for README.MD
enclosure/       # CNC hole-drilling diagram of an off-the-shelf enclosure
firmware/        # Firmware source code
kicad/           # Electronics design
label/           # Keycap labels and branding labels slapped onto the enclosure and cardboard label
manual/          # User manual source and PDF
```

## Ordering a unit

If you speak Toki Pona, it's available via [esun Sate](https://esun.sadale.net/). Please don't order there if you don't speak the language.

I'm also selling/gifting white-labeled units in a few other communities that I care about in small quantities.

This is a hobbyist project. It's all about sharing. Profit-making is not my objective. I intentionally limit my scale of operation to avoid burn out.
Currently I don't have a plan to sell it to the mass public and I'm not sure on if there's even a demand for that.

If you want a unit very badly, you can contact me and tell me your situation. I'll see if I could arrange something for you.

## Building your Own Unit

Please follow the the instruction in the "[assembly/](./assembly/)" folder

# License

This repo contains third-party source code. I do not claim the ownership of these third-party files. Please refer to the license file on the corresponding folders for their licenses:

* ./bootloader/linker_script
* ./bootloader/pff3a
* ./firmware/fatfs

For all other files, I made them. They're released under BSD 2-clause license.
