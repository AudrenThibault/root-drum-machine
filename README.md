# Root-drum-machine

![Alt text](root-drum-machine.jpg?raw=true "Root Drum Machine")

Music video example :   

[![IMAGE ALT TEXT](http://img.youtube.com/vi/fZjF7MMySLA/0.jpg)](https://youtu.be/fZjF7MMySLA "Root Drum Machine")

# Support :

Help me keep creating stuff like this and making albums by buying my music here :
https://mazhootmusic.bandcamp.com/album/dora-world

# Use the provided .nds file

With an emulator or on real nintendo ds (on emulator it's really bad, use the real console)

# Build

Create a folder named maxmod_data with your .wav samples, and type this in a terminal

- make clean
- make

# MOD files
## Warning
Vérifier que les trous laissés par les MOD dans soundbank.h soient gérés. Chercher la ligne struct Trou trous[] = 
Il se peut qu'il faille modifier les valeurs après un make clean et make ou après avoir ajouté d'autres samples.
!!! bien prendre le soundbank.h qui est dans build/soundbank.h !!!
## Add a new mod file :
Place mod files in maxmod_data (where the samples are also).
If errors, it can be just that there is not enough place for the new mods.
chercher la fonction : void modPlay()
et ajouter le mod en fonction de son numéro dans
build/soundbank.h il faut avoir fait aussi ce qui est dans le # Warning juste au dessus.
SYNC mode : malheuresement il n'y a pas de fonction pour modifier le pan global d'un fichier mod dans maxmod donc a priori ce n'est pas possible. Il vaut mieux utiliser syncKontrol sur l'ipad et définir le bpm sur la volca. Ou en réglant directement le tempo sur la volca ça marche.


# About

This project is a fork of a project realised by Tolmdyn, available here : https://github.com/tolmdyn/gba-drum. 
Thanks to him for letting me use his sequencer for my project.

it was at first designed for gameboy advance, but it's now supported on Nintendo DS, using the advantage of "screen buttons" for more "quick access" options.

# Manual :

It works better on the real hardware, it works also on emulators but the fluidity of the sequencer is not very accurate.

## Top screen

### Sequencer

- navigate in the sequencer : up/down/left/right
- A : enter a note (which will play a sample) 
- B : remove a note
- change the options of the notes with A + up/down/left/right pad
- Enter : play/stop the sequencer
- Select : play/stop the sequencer
(try both enter and select if you want to quick start and stop in time, and keep the one which works the best)
- R and L buttons (it's NOT the same buttons as directional pad) to change the sequencers parts. It works but if you combinate it with some bottom screen options, it will make display garbage.
It's accorded with the option "Loop patterns" on the bottom screen. If it's on, then sequences change from 00 to 01 to 02 etc each 1/4 time.

#### Notes options :
##### A + up/down/left/right pad
###### A + up/down

- o : normal volume
- O : loud volume
- R (Random) : sample change randomly at each time it's played. The panoramic is also randomly definate.
- 1/2 : sample is played 1/2 times
- 1/3 : sample is played 1/3 times
- etc... until 1/f : sample is played 1/15 times

###### A + left/right

- A+right : increase the pitch to one semitone
- A+left : dicrease the pitch to one semitone 

#### Pink instructions :

- BPM : change the BPM (tempo) with X and Y button
- Random : press B+A (keep B pressing then press A) : insert notes randomly in the sequencer
- Less steps : press B+left/right to remove columns. It will make the sequencer shorter.
- Change sample : press B+up/down to play the next or the previous sample in maxmod_data folder (blue indication on the left). It will change the sample according to the line where the red cross is.

## Bottom screen (press on the green buttons with the stylet) page 1 :

Works only if the sequencer is not playing to prevent display bugs

- Sync : works with Y cable. For exemple, one minijack to double mono female jacks to split the stereo signal in 2 mono signals. The left signal is the sync and the right signal is the normal sound (so plug it in mixer or headphones). Be carefull ! Sync signal can damage your monitors or your ears so test it at low level for see if you are listening the right signal. 

It works with Korg machines like volcas, and probably on monotribe but you will have to put the level of the nintendo DS at the maximum in order to make it work.

It can works on other machines too, like Akai Rhythm Wolf, but you will have to use a mixer or something like that to help increasing the output level before plugging it back into your machine. 

Warning : Sync works with notes which are on track 11. If you want a normal sync you have to put notes in track 11 on the beggining of each columns like this o--- o--- o--- o---
If Sync is on Off, track 11 is a normal sample track

- IA Random : Randomize the notes on the sequencer each 4 times
- IA crazy bpm : bpm will change randomly each 1/33 times
- Super live : press R and L buttons (it's NOT the same buttons as directional pad) to change the pitch of all samples. It's called super live because it's my favorite feature (with polyrhythm) for live playing.
An indication in yellow will appear at the top right corner.
Warning : press several times on R because the pitch start very low so you will hear nothing if you press only R one time (too low to hear).
In super live mod, B+Up/Down will change all the samples of each lines.
- IA limit : (don't work for now)
- Loop paterns : change sequences from 00 to 01 to 02 etc... each 1/4 times only if the sequences are not empty.
- IA Auto Cymbal : automatically create notes on track 10 each 1/25 times
- Polyrhythm : (best feature in my opinion)
It will cut the sequencer into 2 or 3 sequencers (2 at first and 3 if you press on 3 button which appeared). Move the cursor to be on the first, on the second or on the third and press B+Left (Left while keeping B pressed). It will make the sequencer where the red cross is, shorter than the second, so you can create polyrhythms. Try something simple at first like this :

&nbsp;&nbsp;&nbsp;&nbsp;&nbsp;&nbsp;&nbsp;00&nbsp;&nbsp;&nbsp;&nbsp;&nbsp;  o--- ---- o--- ----  
&nbsp;&nbsp;&nbsp;&nbsp;&nbsp;&nbsp;&nbsp;01&nbsp;&nbsp;&nbsp;&nbsp;&nbsp;  ---- ---- ---- ----

&nbsp;&nbsp;&nbsp;&nbsp;&nbsp;&nbsp;&nbsp;07&nbsp;&nbsp;&nbsp;&nbsp;&nbsp;  o--- ---- ----  
&nbsp;&nbsp;&nbsp;&nbsp;&nbsp;&nbsp;&nbsp;08&nbsp;&nbsp;&nbsp;&nbsp;&nbsp;  ---- ---- ----

&nbsp;&nbsp;&nbsp;&nbsp;&nbsp;&nbsp;&nbsp;and you will hear the magic of polyrhythm :-)

## Bottom screen (press on the green buttons with the stylet) page 2 :

press on the top right arrow ( | => | ) for access page 2

- IA sample change : Change randomly the samples, according to each number you choose. 1 = 1/1, 2 = 1/2, 3 = 1/3 etc...
- IA pitch change : same as before but with the pitch
- IA Arpeggiator : experimental : use R/L button to increase or dicrease repetition of notes, and change pitchs of reapeted notes.


## Don't work (to do) :

- Sometimes, cross and notes are white and not red and green.

- Select + R and L (it's NOT the same buttons as directional pad) for settings. It works but it will cause glitch. This menu come from previous gba drum version and will be probably removed because everything is now accessible in the bottom screen. 

- R and L buttons (it's NOT the same buttons as directional pad) to change the sequencers parts. It works at a certain points but if you combinate it with some bottom screen options, it will make display garbage.
It's accorded with the option "Loop patterns" on the bottom screen. If it's on, then sequences change from 00 to 01 to 02 etc each 1/4 time.

- IA Random : ability to define the times it randomize (1/2, 1/4, 1/5 etc...)

- IA Limit : defines the time (1/2, 1/4 etc...) on IA Random and/or IA crazy bpm will activate.