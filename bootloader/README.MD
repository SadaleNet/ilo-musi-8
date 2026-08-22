**(English: Scroll down for English | toki ike Inli li lon anpa pi toki pona)**

TODO: document the meaning of each of the "numbers" and the smiley

# ilo "Bootloader" li seme?

* kepeken ilo ni la jan li ken pana e sona sin tawa ilo musi 8 kepeken lipu sona "TF Card"
* lipu sona li jo e lipu `ILOMUSI8.BIN` la ilo ni li wile pana e sona tawa ilo musi 8 kepeken ijo insa `ILOMUSI8.BIN`. ali li pona la sitelen `:)` li lon.
* sina ken pana e ilo ni tawa ilo musi 8 kepeken ilo tu ni lon tenpo sama: ilo "WCH-LinkE" en ilo "minichlink". o kepeken nimi ni: `CH32FUN_PATH=/path/to/ch32fun make`
* kin la sina o kepeken nimi ni: `CH32FUN_PATH=/path/to/ch32fun  make optionbytes`. sina kepeken ala nimi ni la ilo ni li pali ala a!
* sina wile ala kepeken ilo `make` la sina kin li ken kepeken ilo `minichlink` kepeken `bootloader.bin`

# Bootloader of ilo musi 8

* Provides mechanism of flashing firmware via TF Card
* Upon detecting `ILOMUSI8.BIN`, the bootloader would flash the file as firmware. Upon success, a smiley `:)` would be displayed on the screen
* To flash, WCH-LinkE and minichlink are required. Run the following command after connecting ilo musi 8 with WCH-LinkE: `CH32FUN_PATH=/path/to/ch32fun make`
* You also need to enable the bootloader with the following command: `CH32FUN_PATH=/path/to/ch32fun make optionbytes`
* Instead of using make, you can flash the bootloader by using the pre-built binary `bootloader.bin` with `minichlink`

![nasin pi pana sona](../docs-assets/programming.jpg)
