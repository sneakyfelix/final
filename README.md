# ambient voices
final project for mat 201 

an interactive audiovisual application that converts spoken language into animated letter agents with emergent flocking behaviors. built using AlloLib with openAi's [Whisper speech recognition](https://openai.com/index/whisper/) modified by Karl for OSC integration.

the application listens to spoken input and spawns individual letter agents for each word. letters initially form into text, then transition into autonomous agents with some flocking behaviors based on character type (i.e. if a letter is A or E). the system creates an ambient soundscape triggered by contextual keywords in the speech.

## instructions on how to use 

**some basic controls** <br> 
--> speak clearly to generate letter agents, ideally say short phrases <br> 
--> say **"freeze"** to pause all animation <br>
--> say **"unfreeze"** to resume movement <br>
--> say **"reset"** to clear all letters from the scene <br> 
--> say **"faster"** or **"slower"** to adjust animation speed (say **"normal"** to reset) <br>
--> say **"clear"** to remove all the letters <br> 

**background colors:** <br> 
--> say any color name to change the background <br> 
--> red, blue, green, yellow, purple, orange, pink, cyan, white, black, grey, brown <br> 

**letter size:** <br> 
--> **"tiny"** = reduced letter size <br> 
--> **"regular"** = default size  <br> 
--> **"huge"** = enlarged letters <br> 

**letter opacity** <br> 
--> **invisible** = cannot see letters <br>
--> **faint** = reduced opacity of letters <br> 
--> **default** = default opacity of letters 

**flocking distance:** <br> 
--> **"close"** = tighter grouping behavior <br> 
--> **"normal"** = default grouping distance <br> 
--> **"spread"** = increased separation between groups <br> 

### ambient sounds

i selected 50 mundane everyday sounds to create contextual audio responses. i sourced these sounds from either my own archive (phone, camera, etc) or from [pixbay's royalty-free sound effect library ](https://pixabay.com/sound-effects/) <br> <br> 

when specific keywords are detected in speech, corresponding environmental sounds are triggered and played. <br> <br>

_i've attached a document with a list of available words for users to speak_ <br> <br> 

**flocking** <br> 

letters spawn in word formations, then after 2-5 seconds receive random initial velocities and begin flocking behaviors. each character type displays unique coloring based on it's charachter type. <br> <br> 

each letter functions as an agent implementing these behavioral forces: <br> 

--> **separation**: maintains minimum distance from other agents <br> 
--> **grouping**: letters of identical characters attract each other  <br> 
--> **alignment**: letters match velocity with nearby similar letters <br> 
--> **boundary containment**: letter remain within defined spatial limits <br> 
--> **random movement**: letters have random movememnt <br> 


<img width="634" alt="Screenshot 2025-06-11 at 14 33 58" src="https://github.com/user-attachments/assets/d693235f-f99d-4d77-b73c-ec1cb846dbcc" />


