#include "formula.hpp"
#include <set>

namespace LTL {
namespace detail {

class Translator {
public:
  Translator() {};
  ~Translator() {};
  FormulaPtr translate(FormulaPtr f);
  
private:
  FormulaPtr translate_p(FormulaPtr f, FormulaPtr& a, std::set<std::string>& plns);
  FormulaPtr conc(FormulaPtr ax, FormulaPtr fr);
  std::string prop_name(FormulaPtr f, std::set<std::string>& plns);
  std::string prop_name_r(FormulaPtr f, std::set<std::string>& plns);
  void init_plns(std::set<std::string>& plns, FormulaPtr f);
  std::set<std::string> plns;
};

}
}