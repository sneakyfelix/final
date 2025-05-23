#include "al/app/al_App.hpp"
#include "al/graphics/al_Font.hpp"
#include "al/math/al_Random.hpp"
#include "al/sound/al_SoundFile.hpp"

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

struct Letter { // add attraction or just initial positions and then target positions when osc message arrives? // letters will form and then drift off but stay on the screen 
    char c;
    Vec3f pos;
  };

class MyApp : public App {
  Font font;
  Mesh mesh, letterMesh;
  std::vector<Letter> letters;
  SoundFilePlayerTS playerTS;

  std::unordered_map<std::string, RGB> color_word{
      {"green", RGB(0.0, 1.0, 0.0)},  {"red", RGB(1.0, 0.0, 0.0)},
      {"blue", RGB(0.0, 0.0, 1.0)},   {"yellow", RGB(1.0, 1.0, 0.0)},
      {"purple", RGB(0.5, 0.0, 0.5)}, {"orange", RGB(1.0, 0.5, 0.0)},
      {"pink", RGB(1.0, 0.75, 0.8)},  {"brown", RGB(0.6, 0.3, 0.2)},
      {"black", RGB(0.0, 0.0, 0.0)},  {"white", RGB(1.0, 1.0, 1.0)}};


  std::unordered_map<std::string, float> size_word{
      {"tiny", 0.3f},  {"huge", 1.0f}};

  std::unordered_map<std::string, std::string> audio_word{
    {"wind", "night.mp3"}}; 
  
  int fontSize = 24;
  float wordHeight = 0.5f; 
  RGB background{0.0, 0.0, 0.0}; 

// currentFont = "data/arial.ttf"; // later for adding other fonts based on word choice ? 

  void onCreate() override {
    nav().pos(0, 0, 10);
    nav().setHome();
    font.load("data/arial.ttf", fontSize, 2048); // 
    font.alignCenter();
    std::string alphabet = "abcdefghijklmnopqrstuvwxyz";
    int totalLetters = 200;

    for (int i = 0; i < totalLetters; ++i) {
        char c = alphabet[rand() % alphabet.size()];
        Vec3f pos = Vec3f(rnd::uniformS(), rnd::uniformS(), 0.0) * 20.0f;
        letters.push_back({c, pos});
  }
    
    font.write(mesh, "(listening...)", 0.3f);
  } 

  void onMessage(osc::Message& m) override {
    //std::cout << "Received message: " << m.addressPattern() << std::endl;
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

        if (audio_word.find(word) != audio_word.end()) {
            // audio_word[word]; // play? get string to play the file name 
        }
      } 

      size_t foundIndex = text.find("alexa");
      if (foundIndex != std::string::npos) {
        // we heard alexa name
      }
      font.write(mesh, text.c_str(), wordHeight);
    } 
  } 

  void onAnimate(double dt) override {}

  void onDraw(Graphics& g) override {
    g.clear(background);
    g.blending(true);
    g.blendTrans();
    g.texture(); 
    // i tried changing the color using g.color(0.0, 0.0, 0.1) but then i would only get blue squares? // am i missing something ? 
    font.tex.bind();

    for (auto& letter : letters) {
        font.write(letterMesh, std::string(1, letter.c).c_str(), 0.3f);
        g.pushMatrix();
        g.translate(letter.pos);
        g.draw(letterMesh);
        g.popMatrix();
      }

    g.draw(mesh);
    font.tex.unbind();
  }
}; 
int main() { MyApp().start(); }
