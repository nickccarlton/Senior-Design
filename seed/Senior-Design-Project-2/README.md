# Senior-Design-Project-2

## Author
This FX chain is designed by 2025 EEE499 Team 5 - Nick Carlton, Joe Liptock, Silas Stevens, Robert Barnett, and Ju Lee. We are mentored by Professor Michael Goryll.

## Description
This FX chain is designed to be flashed to a Daisy Seed module and used with our custom-designed hardware. (Email to request schematics.) 

This FX chain is our "SparkleClean" chain which processes raw_in using digital (d_) and analog (a_) FX to output processed_out in the following signal path:
raw_in -> a_preamp -> d_chorus -> d_cabinet model -> a_distortion -> processed_out.

Our other two FX chains are 

1) Our "Grungy" chain which processes raw_in using digital (d_) and analog (a_) FX in the following signal path:
raw_in -> a_preamp -> d_compressor -> d_LPF -> d_overdrive -> d_reverb -> a_distortion -> processed_out; and

3) Our "Spacious" chain which processes raw_in using digital (d_) and analog (a_) FX in the following signal path:
raw_in -> a_preamp -> d_gate -> d_reverb -> a_distortion -> processed_out.
