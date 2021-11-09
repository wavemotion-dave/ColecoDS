This is highly specialised for the NDS but can probably be tweaked
quite easily to support other platforms as well.

First call SN76496_set_mixrate with 0 or 1 for low or high quality.
Then call SN76496_set_frequency with the actual clock rate of the chip.
Finally call SN76496_init to set it up.

