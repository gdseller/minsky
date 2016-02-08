/*
  @copyright Steve Keen 2012
  @author Russell Standish
  This file is part of Minsky.

  Minsky is free software: you can redistribute it and/or modify it
  under the terms of the GNU General Public License as published by
  the Free Software Foundation, either version 3 of the License, or
  (at your option) any later version.

  Minsky is distributed in the hope that it will be useful,
  but WITHOUT ANY WARRANTY; without even the implied warranty of
  MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
  GNU General Public License for more details.

  You should have received a copy of the GNU General Public License
  along with Minsky.  If not, see <http://www.gnu.org/licenses/>.
*/
#ifndef VARIABLE_H
#define VARIABLE_H

#include "slider.h"
#include "str.h"
#include "group.h"
//#include "clickType.h"

#include <ecolab.h>
#include <arrays.h>

#include <vector>
#include <map>
// override EcoLab's default CLASSDESC_ACCESS macro
#include "classdesc_access.h"

#include "polyBase.h"
#include <polyPackBase.h>
#include "variableType.h"
#include "item.h"
#include <cairo/cairo.h>

namespace minsky
{
  class VariablePtr;
  struct SchemaHelper;

  class VariableBase: public classdesc::PolyBase<VariableType::Type>,
                      virtual public classdesc::PolyPackBase,
                      public Item, public Slider, 
                      public VariableType
  {
  public:
    typedef VariableType::Type Type;
  protected:
 
    friend struct minsky::SchemaHelper;

  private:
    CLASSDESC_ACCESS(VariableBase);
    std::string m_name; 

    /// id of group or godely within which this variable is scoped. -1 = global
    int m_scope;

    /// sets the scope of this variable. Does nothing if scope does not exist
    void setScope(int);

  protected:
    void addPorts();
    
  public:
    ///factory method
    static VariableBase* create(Type type); 

    virtual size_t numPorts() const=0;

    /// @{ variable displayed name
    virtual std::string name() const;
    virtual std::string name(const std::string& nm);
    /// @}

    /// unqualified portion of name
    virtual std::string uqName() const {return m_name;}

    /// ensure an associated variableValue exists
    void ensureValueExists() const;

    /// string used to link to the VariableValue associated with this
    virtual std::string valueId() const;

    /// fully qualified name (marked up with namespace scope)
    virtual std::string fqName() const;

    /// scope (namespace) of this variable. -1==global scope
    int scope() const {return m_scope;}

    /// zoom by \a factor, scaling all widget's coordinates, using (\a
    /// xOrigin, \a yOrigin) as the origin of the zoom transformation
    //   void zoom(float xOrigin, float yOrigin,float factor);
    void setZoom(float factor) {zoomFactor=factor;}

    /// the initial value of this variable
    std::string init() const; /// < return initial value for this variable
    std::string init(const std::string&); /// < set the initial value for this variable

    /// @{ current value associated with this variable
    double value(double);
    double value() const;  
    /// @}

    /// sets variable value (or init value)
    void sliderSet(double x) {init(str(x)); value(x);}
    /// initialise slider bounds when slider first opened
    void initSliderBounds();
    void adjustSliderBounds();

    /// variable is on left hand side of flow calculation
    bool lhs() const {return type()==flow || type()==tempFlow;} 
    /// variable is temporary
    bool temp() const {return type()==tempFlow || type()==undefined;}
    virtual Type type() const=0;
    virtual VariableBase* clone() const=0;
  
    VariableBase() {}
    virtual ~VariableBase() {}

    /// adds inPort for integral case (not relevant elsewhere) if one
    /// not allocated, removes it if one allocated
    // virtual void toggleInPort();

    /** draws the icon onto the given cairo context 
        @return cairo path of icon outline
    */
    //   void draw(cairo_t*) const;

  };

  // a separate class to allow automatic compiler generation of
  // VariableBase assignment to work
//  class VariablePorts: public VariableBase
//  {
//    void delPorts();
//    CLASSDESC_ACCESS(VariablePorts);
//    friend struct minsky::SchemaHelper;
//  protected:
//    void addPorts();
//  public:
//    VariablePorts() {}
//    ~VariablePorts() {delPorts();}
//
//    VariablePorts(const VariablePorts& x): VariableBase(x) {addPorts();}
//    VariablePorts& operator=(const VariablePorts& x) {
//      delPorts();
//      VariableBase::operator=(x);
//      m_ports.clear();
//      addPorts();
//      return *this;
//    }
//    
//    void swapPorts(VariablePorts& v);
//    void toggleInPort();
//  };

  template <VariableType::Type T>
  class Variable: public VariableBase, public classdesc::PolyPack<Variable<T> >
  {
  public:
    typedef VariableBase::Type Type;
    Type type() const {return T;}
    size_t numPorts() const override;

    Variable(const Variable& x): VariableBase(x) {this->addPorts();}
    Variable& operator=(const Variable& x) {
      VariableBase::operator=(x);
      this->addPorts();
      return *this;
    }
    Variable(const std::string& name="") {this->name(name); this->addPorts();}
    Variable* clone() const override {return new Variable(*this);}
  };

  struct VarConstant: public Variable<VariableType::constant>
  {
    int id;
    static int nextId;
    VarConstant(): id(nextId++) {ensureValueExists();}
    std::string valueId() const override {return "constant:"+str(id);}
    std::string name() const override {return init();}
    std::string name(const std::string& nm) override {ensureValueExists(); return name();}
    std::string fqName() const override {return name();}
    std::string uqName() const override {return name();}
    VarConstant* clone() const override {return new VarConstant(*this);}
    
  };

  class VariablePtr: 
    public classdesc::shared_ptr<VariableBase>
  {
    typedef classdesc::shared_ptr<VariableBase> PtrBase;
  public:
    virtual int id() const {return -1;}
    VariablePtr(VariableBase::Type type=VariableBase::undefined, 
                const std::string& name=""): 
      PtrBase(VariableBase::create(type)) {get()->name(name);}
    template <class P>
    VariablePtr(P* var): PtrBase(dynamic_cast<VariableBase*>(var)) 
    {
      // check for incorrect type assignment
      assert(!var || *this);
    }
    VariablePtr(const VariableBase& x): PtrBase(x.clone()) {}
    /// changes type of variable to \a type
    void retype(VariableBase::Type type);
  };

}
#include "variable.cd"
#endif