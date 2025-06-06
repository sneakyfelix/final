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

/// still need to DistributedAppWithState<CommonState>
// add more sounds for words 

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
    float separationTime;      // when to start separating from word formation
    bool isSeparated;          // whether letter has separated from word
    

    // chatgpt created letteragent 
    LetterAgent(char ch, Vec3f position, std::string w, int idx) 
        : c(ch), pos(position), velocity(0,0,0), target(position), separationTime(rnd::uniform(2.0f, 5.0f)), isSeparated(false) {}
          
    void update(float dt, const std::vector<LetterAgent>& allLetters, bool frozen, float speedMultiplier) {
        if (frozen) {
            return; // stop 
        }
        
        float adjustedDt = dt * speedMultiplier;
        separationTime -= adjustedDt;
        
        if (separationTime <= 0 && !isSeparated) {
            isSeparated = true;
            velocity = Vec3f(rnd::uniformS(2.0f), rnd::uniformS(2.0f), 0);
        }
        
        if (isSeparated) {

            // letter behaviors
            Vec3f separation = getSeparation(allLetters);
            Vec3f grouping = getGroupingAttraction(allLetters);
            Vec3f wander = getRandomMoving();
            
            // apply forces
            velocity += (separation * 0.8f + grouping * 1.2f + wander * 0.1f) * speedMultiplier;
            velocity *= 0.98f; // damping
            
            // limit velocity 
            float maxSpeed = 3.0f * speedMultiplier;
            if (velocity.mag() > maxSpeed) {
                velocity = velocity.normalize() * maxSpeed;
            }
            
            pos += velocity * adjustedDt;
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
            steer += perpendicular * 0.3f;
        }
        
        return steer;
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
  
  // frozen and speed 
  bool isFrozen = false;
  float speedMultiplier = 1.0f;

  std::unordered_map<std::string, RGB> color_word{
    {"red", RGB(1.0, 0.0, 0.0)}, {"green", RGB(0.0, 1.0, 0.0)},
    {"blue", RGB(0.0, 0.0, 1.0)}, {"yellow", RGB(1.0, 1.0, 0.0)},
    {"purple", RGB(0.5, 0.0, 0.5)}, {"orange", RGB(1.0, 0.5, 0.0)},
    {"pink", RGB(1.0, 0.4, 0.7)}, {"cyan", RGB(0.0, 1.0, 1.0)},
    {"white", RGB(1.0, 1.0, 1.0)}, {"black", RGB(0.0, 0.0, 0.0)},
    {"grey", RGB(0.5, 0.5, 0.5)}, {"brown", RGB(0.6, 0.3, 0.1)},};

  std::unordered_map<std::string, float> size_word{
      {"tiny", 0.3f}, {"huge", 1.0f}, {"regular", 0.6f}, };



// need to create find words 

  std::unordered_map<std::string, float> distance_word{
      {"close", 2.0f}, {"near", 4.0f}, 
      {"far", 8.0f}, {"distant", 12.0f}, 
      {"tight", 1.5f}, {"spread", 15.0f}, };

  int fontSize = 24;
  float wordHeight = 0.5f; 
  RGB background{0.0, 0.0, 0.0}; 

  void onCreate() override {
    nav().pos(0, 0, 10);
    nav().setHome();
    font.load("arial.ttf", fontSize, 2048);
    font.alignCenter();
  } 

  void onMessage(osc::Message& m) override {
    if (m.addressPattern() == "/whisper") {
      std::string text;
      m >> text;
      std::transform(text.begin(), text.end(), text.begin(), ::tolower);
      text.erase(std::remove_if(text.begin(), text.end(), ::ispunct), text.end());
      
      for (auto& word : lineToWords(text)) {
        if (color_word.find(word) != color_word.end()) {
          background = color_word[word];
        }
        if (size_word.find(word) != size_word.end()) {
          wordHeight = size_word[word]; 
        }
     // if (distance_word.find(word) != distance_word.end()) {
      //  groupdist = distance_word[word];
    //}
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
          speedMultiplier = std::max(speedMultiplier * 0.67f, 0.1f); 
      }
      
      if (text.find("normal") != std::string::npos) {
          speedMultiplier = 1.0f;
      }
        
      // sounds
      if (text.find("bath") != std::string::npos || text.find("water") != std::string::npos || text.find("waves") != std::string::npos || text.find("shore") != std::string::npos) {
          player.load("wave.wav"); 
      }
      if (text.find("park") != std::string::npos || text.find("children") != std::string::npos || text.find("kids") != std::string::npos || text.find("playing") != std::string::npos) {
          player.load("kids.wav"); 
      }
      if (text.find("squirrels") != std::string::npos) {
          player.load("squirrel.wav"); 
      }

      if (text.find("public transportation") != std::string::npos || text.find("public transportation") != std::string::npos || text.find("train") != std::string::npos) {
        player.load("train.wav"); 
      }

      if (text.find("in the car") != std::string::npos || text.find("driving") != std::string::npos || text.find("cars") != std::string::npos) {
        player.load("turnsignal.wav"); 
    }

      if (text.find("called") != std::string::npos) {
        player.load("vibrate.wav"); 
      }

      if (text.find("calling") != std::string::npos) {
        player.load("phonecall.wav"); 
      }

      if (text.find("find") != std::string::npos) {
        player.load("search.wav"); 
      }

      if (text.find("hungry") != std::string::npos) {
        player.load("hungry.wav"); 
      }

      // creates letter agents for each word if not frozen
      if (!isFrozen) {
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
      
      // colors work but are still blocked charachters? 
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
      
      g.color(letterColor);
      
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
      float s = player() * 0.8f;
      level = 0.997f * level + 0.003f * s * s;
      io.out(0) = s;
      io.out(1) = s;
    }
  }
}; 

int main() { 
    MyApp app;
    app.configureAudio(44100, 512, 2, 2);
    app.start(); 
}