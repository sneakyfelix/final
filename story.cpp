#include "al/app/al_App.hpp"
#include "al/graphics/al_Font.hpp"
#include "al/math/al_Random.hpp"
#include "Gamma/SamplePlayer.h"
#include "al/io/al_File.hpp"
#include "al/graphics/al_Image.hpp"
#include "al/app/al_App.hpp"
#include "al/graphics/al_Shapes.hpp"
#include "al/io/al_File.hpp"
#include "al/app/al_DistributedApp.hpp"
#include "al/app/al_GUIDomain.hpp"
#include "al_ext/statedistribution/al_CuttleboneDomain.hpp"
#include "al_ext/statedistribution/al_CuttleboneStateSimulationDomain.hpp"
#include <fstream>
#include <set>
#include <algorithm>


struct CommonState {
    char data[66000];  // larger than a UDP packet
    int frame;
    float signal;
  };


using namespace al;

std::vector<std::string> lineToWords(const std::string& line) {
    std::vector<std::string> words;
    std::stringstream ss(line);
    std::string word;
    while (ss >> word) {
        words.push_back(word);
    }
    return words;
}

struct LetterAgent {
    char c;
    Vec3f pos;
    Vec3f velocity;
    Vec3f target;           
    Vec3f groupDirection;   
    float separationTime;      // when to start separating from word formation
    bool isSeparated;   
    float groupDist; 
    

    // chatgpt created letteragent 
    LetterAgent(char ch, Vec3f position, std::string w, int idx) 
        : c(ch), pos(position), velocity(0,0,0), target(position), 
          groupDirection(0,0,0), separationTime(rnd::uniform(2.0f, 5.0f)), isSeparated(false) {}
          
    void update(float dt, const std::vector<LetterAgent>& allLetters, bool frozen, float speedMultiplier) {
        if (frozen) {
            return; // stop 
        }
        
        float adjustedDt = dt * speedMultiplier;
        separationTime -= adjustedDt;
        
        if (separationTime <= 0 && !isSeparated) {
            isSeparated = true;
            velocity = Vec3f(rnd::uniformS(2.0f), rnd::uniformS(2.0f), 0);

            // random group direction randomly 
            groupDirection = Vec3f(rnd::uniformS(1.0f), rnd::uniformS(1.0f), 0).normalize();
        }
        
        if (isSeparated) {

            // individual behaviors 
            Vec3f separation = getSeparation(allLetters);
            Vec3f grouping = getGroupingAttraction(allLetters);
            Vec3f wander = getRandomMoving();

            // flocking as a group 
            Vec3f alignment = getAlignment(allLetters);
            Vec3f groupMovement = getGroupMovement();
            Vec3f boundaryAvoidance = getBoundaryAvoidance();
            
            // forces 
            velocity += (separation * 0.8f + 
                        grouping * 0.6f + 
                        alignment * 0.4f + 
                        groupMovement * 0.3f + 
                        boundaryAvoidance * 1.0f + 
                        wander * 0.1f) * speedMultiplier;
            
            velocity *= 0.98f; // damping
            
            // limit velocity 
            float maxSpeed = 3.0f * speedMultiplier;
            if (velocity.mag() > maxSpeed) {
                velocity = velocity.normalize() * maxSpeed;
            }
            
            pos += velocity * adjustedDt;
            
            // randomizde the groups flocking direction
            groupDirection += Vec3f(rnd::uniformS(0.02f), rnd::uniformS(0.02f), 0);
            groupDirection = groupDirection.normalize();
            
        } else {
            // stay in word formation and move toward target
            pos = pos.lerp(target, 0.02f * speedMultiplier);
        }
    }
    
    Vec3f getSeparation(const std::vector<LetterAgent>& others) {
        Vec3f steer(0, 0, 0);
        int count = 0;
        float separationRadius = 1.0f;
        
        for (const auto& other : others) {
            float d = (pos - other.pos).mag();
            if (d > 0 && d < separationRadius) {
                Vec3f diff = (pos - other.pos).normalize();
                diff /= d; // weight by distance
                steer += diff;
                count++;
            }
        }
        
        if (count > 0) {
            steer /= count;
            steer = steer.normalize() * 2.0f; // max separation force
        }
        
        return steer;
    }
    
    Vec3f getGroupingAttraction(const std::vector<LetterAgent>& others) {
        Vec3f steer(0, 0, 0);
        int count = 0;
        float groupingRadius = 8.0f;
        
        // finds center of all letters with the same character
        Vec3f center(0, 0, 0);
        for (const auto& other : others) {
            if (other.c == c && other.isSeparated) {
                float d = (pos - other.pos).mag();
                if (d < groupingRadius) {
                    center += other.pos;
                    count++;
                }
            }
        }
        
        if (count > 1) { // only group if there are other similar letters nearby
            center /= count;
            Vec3f desired = (center - pos).normalize() * 1.5f;
            steer = desired;
            
            // circular motion around the group center
            Vec3f toCenter = center - pos;
            Vec3f perpendicular = Vec3f(-toCenter.y, toCenter.x, 0).normalize();
            steer += perpendicular * (0.3f);
        }
        
        return steer;
    }
    
    // alignment behavior for flocking
    Vec3f getAlignment(const std::vector<LetterAgent>& others) {
        Vec3f avgVelocity(0, 0, 0);
        int count = 0;
        float alignmentRadius = 1.0f;
        
        // average velocity of nearby letters with same character
        for (const auto& other : others) {
            if (other.c == c && other.isSeparated) {
                float d = (pos - other.pos).mag();
                if (d > 0 && d < alignmentRadius) {
                    avgVelocity += other.velocity;
                    count++;
                }
            }
        }
        
        if (count > 0) {
            avgVelocity /= count;
            // steer towards average velocity
            Vec3f steer = avgVelocity - velocity;
            return steer * 0.5f; // slower 
        }
        
        return Vec3f(0, 0, 0);
    }
    
    Vec3f getGroupMovement() {
        // wandering group movement 
        return groupDirection * 0.8f;
    }
    
    // boundary for words 
    Vec3f getBoundaryAvoidance() {
        Vec3f center(0, 0, 0);  // center of sphere
        float radius = 8.0f;    // sphere radius
        
        Vec3f toCenter = center - pos;
        float distance = toCenter.mag();
        
        // if letter is outside the sphere push back toward center
        if (distance > radius) {
            return toCenter.normalize() * 2.0f; // push back
        }
        
        // if letter is close to edge push back toward center
        if (distance > radius * 0.9f) {
            float pushStrength = (distance - radius * 0.8f) / (radius * 0.2f);
            return toCenter.normalize() * pushStrength;
        }
        
        return Vec3f(0, 0, 0); // no force if not the above conditions 
    }
    
    Vec3f getRandomMoving() {
        return Vec3f(rnd::uniformS(0.2f), rnd::uniformS(0.2f), 0);
    }
};

class MyApp : public App {
  Font font;
  Mesh mesh, mesh2; 
  gam::SamplePlayer<float, gam::ipl::Linear> player;
  std::vector<LetterAgent> letterAgents;
  std::string filename;

  float level = 0.0f;
  float globalTime = 0.0f;
  
  bool isFrozen = false;
  float speedMultiplier = 1.0f;
  float groupDist = 8.0f; 

  float letterOpacity = 1.0f;

  bool isAudioPlaying = false;

  // background variations 

  std::unordered_map<std::string, RGB> color_word{
    {"red", RGB(1.0, 0.0, 0.0)}, {"green", RGB(0.0, 1.0, 0.0)},
    {"blue", RGB(0.0, 0.0, 1.0)}, {"yellow", RGB(1.0, 1.0, 0.0)},
    {"purple", RGB(0.5, 0.0, 0.5)}, {"orange", RGB(1.0, 0.5, 0.0)},
    {"pink", RGB(1.0, 0.4, 0.7)}, {"cyan", RGB(0.0, 1.0, 1.0)},
    {"white", RGB(1.0, 1.0, 1.0)}, {"black", RGB(0.0, 0.0, 0.0)},
    {"grey", RGB(0.5, 0.5, 0.5)}, {"brown", RGB(0.6, 0.3, 0.1)},};

  // text variations 

  std::unordered_map<std::string, float> size_word{
      {"tiny", 0.3f}, {"huge", 1.0f}, {"regular", 0.6f}, };

  std::unordered_map<std::string, float> opacity_word{
      {"invisible", 0.1f}, {"faint", 0.5f}, {"normal", 1.0f}, };

    // distance needed to attract same letter 
  std::unordered_map<std::string, float> distance_word{
      {"close", 2.0f}, {"normal", 8.0f},  {"spread", 15.0f}, };




  int fontSize = 24;
  float wordHeight = 0.5f; 
  RGB background{0.0, 0.0, 0.0}; 

  void onCreate() override {
    nav().pos(0, 3, 30);
    nav().setHome();
    font.load("arial.ttf", fontSize, 2048);
    font.alignCenter();
  } 

  void onMessage(osc::Message& m) override {

    if (isAudioPlaying) {
      return; // ignore the osc message
    }    

    if (m.addressPattern() == "/whisper") {
      std::string text;
      m >> text;

      if (text.find("[BLANK_AUDIO]") != std::string::npos) {
        return; // ignore if blank audio 
      }

      std::transform(text.begin(), text.end(), text.begin(), ::tolower);
      text.erase(std::remove_if(text.begin(), text.end(), ::ispunct), text.end());
      
      for (auto& word : lineToWords(text)) {
        if (color_word.find(word) != color_word.end()) {
          background = color_word[word];
        }

        if (size_word.find(word) != size_word.end()) {
          wordHeight = size_word[word]; 
        }
        
        if (distance_word.find(word) != distance_word.end()) {
         groupDist = distance_word[word];
        }

        if (opacity_word.find(word) != opacity_word.end()) {
          letterOpacity = opacity_word[word];
        }
    }

      // speed 
      if (text.find("freeze") != std::string::npos) {
          isFrozen = true;
      }
      
      if (text.find("unfreeze") != std::string::npos) {
          isFrozen = false;
      }
      
      if (text.find("faster") != std::string::npos) {
          speedMultiplier = std::min(speedMultiplier * 1.5f, 5.0f); // can only go up to 5x
      }
      
      if (text.find("slower") != std::string::npos) {
          speedMultiplier = std::max(speedMultiplier * 0.67f, 0.1f); // maxing speed
      }
      
      if (text.find("normal") != std::string::npos) {
          speedMultiplier = 1.0f;
      }

      if (text.find("reset") != std::string::npos) {
        letterAgents.clear(); 
    }
        
      // sounds

      bool shouldPlaySound = false;


      if (text.find("bath") != std::string::npos || text.find("water") != std::string::npos || text.find("waves") != std::string::npos || text.find("shore") != std::string::npos) {
          player.load("wave.wav"); 
          shouldPlaySound = true;
      }
      else if (text.find("park") != std::string::npos || text.find("children") != std::string::npos || text.find("kids") != std::string::npos || text.find("playing") != std::string::npos) {
          player.load("kids.wav"); 
          shouldPlaySound = true;
      }
      else if (text.find("squirrels") != std::string::npos) {
          player.load("squirrel.wav"); 
          shouldPlaySound = true;
      }

      else if (text.find("public transportation") != std::string::npos || text.find("public transportation") != std::string::npos || text.find("train") != std::string::npos) {
        player.load("train.wav"); 
        shouldPlaySound = true;
      }

      else if (text.find("in the car") != std::string::npos || text.find("driving") != std::string::npos || text.find("cars") != std::string::npos) {
        player.load("turnsignal.wav");
        shouldPlaySound = true; 
    }

      else if (text.find("called") != std::string::npos) {
        player.load("vibrate.wav"); 
        shouldPlaySound = true;
      }

      else if (text.find("calling") != std::string::npos) {
        player.load("phonecall.wav"); 
        shouldPlaySound = true;
      }

      else if (text.find("find") != std::string::npos) {
        player.load("search.wav"); 
        shouldPlaySound = true;
      } 

      else if (text.find("typing") != std::string::npos || text.find("keyboard") != std::string::npos || text.find("computer") != std::string::npos) {
        player.load("clicking-keyboard.wav");
        shouldPlaySound = true;
    }
    
      else if (text.find("pen") != std::string::npos || text.find("writing") != std::string::npos || text.find("click") != std::string::npos) {
        player.load("clicking-pen.wav");
        shouldPlaySound = true;
    }
    
      else if (text.find("coffee") != std::string::npos || text.find("brewing") != std::string::npos || text.find("machine") != std::string::npos) {
        player.load("coffee-machine.wav");
        shouldPlaySound = true;
    }
    
      else if (text.find("cutting") != std::string::npos || text.find("chopping") != std::string::npos || text.find("vegetables") != std::string::npos || text.find("fruit") != std::string::npos) {
        player.load("cutfruitveg.wav");
        shouldPlaySound = true;
    }
    
      else if (text.find("door") != std::string::npos || text.find("keys") != std::string::npos || text.find("unlocking") != std::string::npos) {
        player.load("door-unlocking-with-keys.wav");
        shouldPlaySound = true;
    }
    
      else if (text.find("drawer") != std::string::npos || text.find("opening") != std::string::npos || text.find("cabinet") != std::string::npos) {
        player.load("drawer-opening.wav");
        shouldPlaySound = true;
    }
    
      else if (text.find("drawing") != std::string::npos || text.find("sketching") != std::string::npos || text.find("art") != std::string::npos) {
        player.load("drawing.wav");
        shouldPlaySound = true;
    }
    
      else if (text.find("fire") != std::string::npos || text.find("flames") != std::string::npos || text.find("burning") != std::string::npos) {
        player.load("fire.wav");
        shouldPlaySound = true;
    }
    
      else if (text.find("fishing") != std::string::npos || text.find("reel") != std::string::npos || text.find("casting") != std::string::npos) {
        player.load("fishing-reel.wav");
        shouldPlaySound = true;
    }
    
      else if (text.find("stove") != std::string::npos || text.find("gas") != std::string::npos || text.find("cooking") != std::string::npos) {
        player.load("gasstove.wav");
        shouldPlaySound = true;
    }
    
      else if (text.find("cleaning") != std::string::npos || text.find("glass") != std::string::npos || text.find("window") != std::string::npos) {
        player.load("glass-cleaning-squeak.wav");
        shouldPlaySound = true;
    }
    
      else if (text.find("grocery") != std::string::npos || text.find("freezer") != std::string::npos || text.find("store") != std::string::npos) {
        player.load("grocery-store-freezer-door.wav");
        shouldPlaySound = true;
    }
    
      else if (text.find("guitar") != std::string::npos || text.find("tuning") != std::string::npos || text.find("strings") != std::string::npos) {
        player.load("guitartuning.wav");
        shouldPlaySound = true;
    }
    
      else if (text.find("heartbeat") != std::string::npos || text.find("heart") != std::string::npos || text.find("pulse") != std::string::npos) {
        player.load("heartbeat.wav");
        shouldPlaySound = true;
    }
    
      else if (text.find("horses") != std::string::npos || text.find("riding") != std::string::npos) {
        player.load("horses-kids.wav");
        shouldPlaySound = true;
    }
    
      else if (text.find("laundry") != std::string::npos || text.find("washing") != std::string::npos || text.find("clothes") != std::string::npos) {
        player.load("laundry.wav");
        shouldPlaySound = true;
    }
    
      else if (text.find("market") != std::string::npos || text.find("crowd") != std::string::npos || text.find("busy") != std::string::npos) {
        player.load("marketnoise.wav");
        shouldPlaySound = true;
    }
    
      else if (text.find("microwave") != std::string::npos || text.find("heating") != std::string::npos || text.find("beeping") != std::string::npos) {
        player.load("microwave.wav");
        shouldPlaySound = true;
    }
    
      else if (text.find("soda") != std::string::npos || text.find("can") != std::string::npos || text.find("fizzy") != std::string::npos) {
        player.load("opening-a-fizzy-can.wav");
        shouldPlaySound = true;
    }
    
      else if (text.find("pills") != std::string::npos || text.find("bottle") != std::string::npos || text.find("medicine") != std::string::npos) {
        player.load("opening-pill-bottle.wav");
        shouldPlaySound = true;
    }
    
      else if (text.find("peeling") != std::string::npos || text.find("wood") != std::string::npos || text.find("scraping") != std::string::npos) {
        player.load("peeling-wood.wav");
        shouldPlaySound = true;
    }
    
      else if (text.find("cards") != std::string::npos || text.find("playing") != std::string::npos || text.find("shuffling") != std::string::npos) {
        player.load("playingcards.wav");
        shouldPlaySound = true;
    }
    
      else if (text.find("rain") != std::string::npos || text.find("raining") != std::string::npos || text.find("storm") != std::string::npos) {
        player.load("rain-sounds.wav");
        shouldPlaySound = true;
    }
    
      else if (text.find("rolling") != std::string::npos || text.find("wheel") != std::string::npos || text.find("ball") != std::string::npos) {
        player.load("rolling.wav");
        shouldPlaySound = true;
    }
    
      else if (text.find("running") != std::string::npos || text.find("jogging") != std::string::npos || text.find("exercise") != std::string::npos) {
        player.load("running.wav");
        shouldPlaySound = true;
    }
    
      else if (text.find("eggs") != std::string::npos || text.find("scrambled") != std::string::npos || text.find("cooking") != std::string::npos) {
        player.load("scrambled-egg.wav");
        shouldPlaySound = true;
    }
    
      else if (text.find("brushing") != std::string::npos || text.find("teeth") != std::string::npos || text.find("sink") != std::string::npos) {
        player.load("sink-and-toothbrush.wav");
        shouldPlaySound = true;
    }
    
      else if (text.find("skateboard") != std::string::npos || text.find("skating") != std::string::npos || text.find("wheels") != std::string::npos) {
        player.load("skateboard.wav");
        shouldPlaySound = true;
    }
    
      else if (text.find("spray") != std::string::npos || text.find("paint") != std::string::npos || text.find("graffiti") != std::string::npos) {
        player.load("spray-paint-rattle-and-spray.wav");
        shouldPlaySound = true;
    }
    
      else if (text.find("stairs") != std::string::npos || text.find("jumping") != std::string::npos || text.find("steps") != std::string::npos) {
        player.load("stairs-jumping.wav");
        shouldPlaySound = true;
    }
    
      else if (text.find("stapler") != std::string::npos || text.find("stapling") != std::string::npos || text.find("office") != std::string::npos) {
        player.load("stapler-sound.wav");
        shouldPlaySound = true;
    }
    
      else if (text.find("gravel") != std::string::npos || text.find("stone") != std::string::npos || text.find("road") != std::string::npos) {
        player.load("stone-road.wav");
        shouldPlaySound = true;
    }
    
      else if (text.find("tapping") != std::string::npos || text.find("fingers") != std::string::npos || text.find("drumming") != std::string::npos) {
        player.load("tapping-fingers.wav");
        shouldPlaySound = true;
    }
    
      else if (text.find("thunder") != std::string::npos || text.find("lightning") != std::string::npos || text.find("storm") != std::string::npos) {
        player.load("thunder.wav");
        shouldPlaySound = true;
    }
    
      else if (text.find("toaster") != std::string::npos || text.find("toast") != std::string::npos || text.find("bread") != std::string::npos) {
        player.load("toaster.wav");
        shouldPlaySound = true;
    }
    
      else if (text.find("toy") != std::string::npos || text.find("guitar") != std::string::npos || text.find("music") != std::string::npos) {
        player.load("toy-guitar-playing.wav");
        shouldPlaySound = true;
    }
    
      else if (text.find("city") != std::string::npos || text.find("urban") != std::string::npos || text.find("traffic") != std::string::npos) {
        player.load("traffic-in-city.wav");
        shouldPlaySound = true;
    }
    
      else if (text.find("walking") != std::string::npos || text.find("footsteps") != std::string::npos || text.find("steps") != std::string::npos) {
        player.load("walking.wav");
        shouldPlaySound = true;
    }
    
      else if (text.find("window") != std::string::npos || text.find("opening") != std::string::npos || text.find("fresh air") != std::string::npos) {
        player.load("window-opening.wav");
        shouldPlaySound = true;
    }
    
      else if (text.find("wine") != std::string::npos || text.find("bottle") != std::string::npos || text.find("cork") != std::string::npos) {
        player.load("winebottle.wav");
        shouldPlaySound = true;
    }
      
      else if (text.find("hungry") != std::string::npos) {
        player.load("hungry.wav"); 
        shouldPlaySound = true;
      }

      if (shouldPlaySound) {
        player.reset(); // reset playback so it plays from the beginning
        isAudioPlaying = true; // wont allow messages to be sent if true
    }

      // creates letter agents for each word if not frozen
      if (!isFrozen && !isAudioPlaying) {
        addWordsAsLetterAgents(text);
    }
    }
  } 
  
  void addWordsAsLetterAgents(const std::string& text) {
    std::vector<std::string> words = lineToWords(text);
    float letterSpacing = 0.6f;
    float startY = 2.0f;
    
    //chatgpt figured out wordspacing 
    for (int w = 0; w < words.size(); w++) {
      std::string word = words[w];
      float wordWidth = word.length() * letterSpacing;
      float startX = -wordWidth / 2.0f;
      
      // positions each word
      Vec3f wordPos(0, startY - w * 1.0f, 0);
      
      for (int i = 0; i < word.length(); i++) {
        Vec3f letterPos = wordPos + Vec3f(startX + i * letterSpacing, 0, 0);
        LetterAgent agent(word[i], letterPos, word, i);
        agent.target = letterPos; // start at word formation position
        letterAgents.push_back(agent);
      }
    }
  }

  void onAnimate(double dt) override {
    if (!isFrozen) {
        globalTime += dt * speedMultiplier;
    }
    
    // update all letter agents
    for (auto& agent : letterAgents) {
      agent.update(dt, letterAgents, isFrozen, speedMultiplier);
    }
  }

  void onDraw(Graphics& g) override {
    g.clear(background);
    g.blending(true);
    g.blendTrans();
    g.texture(); 
    font.tex.bind();

    for (auto& agent : letterAgents) {
      std::string letterStr(1, agent.c);
      font.write(mesh, letterStr.c_str(), wordHeight);
      
      // colors work but are still blocked charachters? but i kind of like?
      RGB letterColor;
      if (agent.isSeparated) {
        switch(agent.c) {
          case 'a': letterColor = RGB(1.0f, 0.0f, 0.0f); break;   // bright red
          case 'b': letterColor = RGB(0.0f, 0.0f, 1.0f); break;   // bright blue
          case 'c': letterColor = RGB(0.0f, 1.0f, 0.0f); break;   // bright green
          case 'd': letterColor = RGB(1.0f, 1.0f, 0.0f); break;   // yellow
          case 'e': letterColor = RGB(1.0f, 0.0f, 1.0f); break;   // magenta
          case 'f': letterColor = RGB(0.0f, 1.0f, 1.0f); break;   // cyan
          case 'g': letterColor = RGB(1.0f, 0.5f, 0.0f); break;   // orange
          case 'h': letterColor = RGB(0.5f, 0.0f, 1.0f); break;   // purple
          case 'i': letterColor = RGB(1.0f, 0.0f, 0.5f); break;   // hot pink
          case 'j': letterColor = RGB(0.5f, 1.0f, 0.0f); break;   // lime green
          case 'k': letterColor = RGB(0.0f, 0.5f, 1.0f); break;   // sky blue
          case 'l': letterColor = RGB(1.0f, 0.8f, 0.0f); break;   // yellow gold
          case 'm': letterColor = RGB(0.8f, 0.0f, 0.8f); break;   // purple
          case 'n': letterColor = RGB(0.0f, 0.8f, 0.8f); break;   // teal
          case 'o': letterColor = RGB(1.0f, 0.3f, 0.3f); break;   // coral
          case 'p': letterColor = RGB(0.3f, 1.0f, 0.3f); break;   // light Green
          case 'q': letterColor = RGB(0.3f, 0.3f, 1.0f); break;   // light Blue
          case 'r': letterColor = RGB(0.8f, 0.4f, 0.0f); break;   // brown
          case 's': letterColor = RGB(0.6f, 0.0f, 0.6f); break;   // dark purple
          case 't': letterColor = RGB(0.0f, 0.6f, 0.6f); break;   // dark teal
          case 'u': letterColor = RGB(1.0f, 0.6f, 0.8f); break;   // pink
          case 'v': letterColor = RGB(0.6f, 1.0f, 0.8f); break;   // mint
          case 'w': letterColor = RGB(0.8f, 0.6f, 1.0f); break;   // lavender
          case 'x': letterColor = RGB(1.0f, 0.2f, 0.8f); break;   // deep pink
          case 'y': letterColor = RGB(0.8f, 1.0f, 0.2f); break;   // yellow green
          case 'z': letterColor = RGB(0.2f, 0.8f, 1.0f); break;   // light cyan
          default:  letterColor = RGB(1.0f, 1.0f, 1.0f); break;   // white 
        }
        
      } 
      
      g.color(letterColor.r, letterColor.g, letterColor.b, letterOpacity);
      
      //  mesh to agent's position
      for (auto& v : mesh.vertices()) {
        v += agent.pos;
      }
      
      g.draw(mesh);
    }
    
    font.tex.unbind();
  }

  void onSound(AudioIOData& io) override {
    while (io()) {
        float s = 0.0f;
        
        if (isAudioPlaying) {
            s = player() * 0.8f;
            
            // check if the sample has finished playing
            if (player.pos() >= player.frames() - 1) {
                isAudioPlaying = false; // allow new messages? no
                player.reset(); // reset for next time
            }
        }
        
        level = 0.997f * level + 0.003f * s * s;
        io.out(0) = s;
        io.out(1) = s;
    }
}}; 

int main() { 
    MyApp app;
    app.configureAudio(44100, 512, 2, 2);
    app.start(); 
}
