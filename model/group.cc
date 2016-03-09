/*
  @copyright Steve Keen 2015
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

#include "group.h"
#include "wire.h"
#include "operation.h"
#include "minsky.h"
#include <cairo_base.h>
#include <ecolab_epilogue.h>
using namespace std;
using namespace ecolab::cairo;

namespace minsky
{
  Group& GroupPtr::operator*() const {return dynamic_cast<Group&>(ItemPtr::operator*());}
  Group* GroupPtr::operator->() const {return dynamic_cast<Group*>(ItemPtr::operator->());}

  ItemPtr Group::removeItem(const Item& it)
  {
    for (auto i=items.begin(); i!=items.end(); ++i)
      if (i->get()==&it)
        {
          ItemPtr r=*i;
          items.erase(i);
          return r;
        }

    for (auto& g: groups)
      if (ItemPtr r=g->removeItem(it))
        return r;
    return ItemPtr();
  }
       
  WirePtr Group::removeWire(const Wire& w)
  {
    for (auto i=wires.begin(); i!=wires.end(); ++i)
      if (i->get()==&w)
        {
          WirePtr r=*i;
          wires.erase(i);
          return r;
        }

    for (auto& g: groups)
      if (WirePtr r=g->removeWire(w))
        return r;
    return WirePtr();
  }

  GroupPtr Group::removeGroup(const Group& group)
  {
    for (auto i=groups.begin(); i!=groups.end(); ++i)
      if (i->get()==&group)
        {
          GroupPtr r=*i;
          groups.erase(i);
          return r;
        }

    for (auto& g: groups)
      if (GroupPtr r=g->removeGroup(group))
        return r;
    return GroupPtr();
  }
       
  ItemPtr Group::findItem(const Item& it) const 
  {
    // start by looking in the group it thnks it belongs to
    if (auto g=it.group.lock())
      if (g.get()!=this) 
        {
          auto i=g->findItem(it);
          if (i) return i;
        }
    return findAny(&Group::items, [&](const ItemPtr& x){return x.get()==&it;});
  }


  ItemPtr Group::addItem(const shared_ptr<Item>& it)
  {
    if (auto x=dynamic_pointer_cast<Group>(it))
      return addGroup(x);
   
    auto origGroup=it->group.lock();
    if (origGroup && origGroup.get()!=this)
      origGroup->removeItem(*it);

    it->group=self.lock();

    // move wire to highest common group
    // TODO add in I/O variables if needed
    for (auto& p: it->ports)
      {
        assert(p);
        for (auto& w: p->wires)
          {
            assert(w);
            adjustWiresGroup(*w);
          }
      }

    // need to deal with integrals especially because of the attached variable
    if (auto intOp=dynamic_cast<IntOp*>(it.get()))
      if (intOp->getIntVar())
        if (auto oldG=intOp->getIntVar()->group.lock())
          {
            if (oldG.get()!=this)
              addItem(oldG->removeItem(*intOp->getIntVar()));
          }
        else
          addItem(intOp->intVar);
            
    items.push_back(it);
    return items.back();
  }

  void Group::adjustWiresGroup(Wire& w)
  {
    // Find common ancestor group, and move wire to it
    assert(w.from() && w.to());
    shared_ptr<Group> p1=w.from()->item.group.lock(), p2=w.to()->item.group.lock();
    assert(p1 && p2);
    unsigned l1=p1->level(), l2=p2->level();
    for (; l1>l2; l1--) p1=p1->group.lock();
    for (; l2>l1; l2--) p2=p2->group.lock();
    while (p1!=p2) 
      {
        assert(p1 && p2);
        p1=p1->group.lock();
        p2=p2->group.lock();
      }
    w.moveIntoGroup(*p1);
  }

  void Group::moveContents(Group& source) {
     if (&source!=this)
       {
         for (auto& i: source.groups)
           if (i->higher(*this))
             throw error("attempt to move a group into itself");
          for (auto& i: source.items)
            addItem(i);
          for (auto& i: source.groups)
            addGroup(i);
          /// no need to move wires, as these are handled above
          source.clear();
       }
  }

  GroupPtr Group::addGroup(const std::shared_ptr<Group>& g)
  {
    auto origGroup=g->group.lock();
    if (origGroup && origGroup.get()!=this)
      origGroup->removeGroup(*g);
    g->group=self;
    g->self=g;
    groups.push_back(g);
    return groups.back();
  }

  WirePtr Group::addWire(const std::shared_ptr<Wire>& w)
  {
    wires.push_back(w);
    return wires.back();
  }

  bool Group::higher(const Group& x) const
  {
    //if (!x) return false; // global group x is always higher
    for (auto i: groups)
      if (i.get()==&x) return true;
    for (auto i: groups)
      if (higher(*i))
        return true;
    return false;
  }

  unsigned Group::level() const
  {
    if (auto g=group.lock())
      return g->level()+1;
    else
      return 0;
  }

  namespace
  {
    template <class G>
    G& globalGroup(G& start)
    {
      auto g=&start;
      while (auto g1=g->group.lock())
        g=g1.get();
      return *g;
    }
  }

  const Group& Group::globalGroup() const
  {return minsky::globalGroup(*this);}
  Group& Group::globalGroup()
  {return minsky::globalGroup(*this);}


  bool Group::uniqueItems(set<void*>& idset) const
  {
    for (auto& i: items)
      if (!idset.insert(i.get()).second) return false;
    for (auto& i: wires)
      if (!idset.insert(i.get()).second) return false;
    for (auto& i: groups)
      if (!idset.insert(i.get()).second || !i->uniqueItems(idset)) 
        return false;
    return true;
  }

  float Group::contentBounds(double& x0, double& y0, double& x1, double& y1) const
  {
    float localZoom=1;
#ifndef CAIRO_HAS_RECORDING_SURFACE
#error "Please upgrade your cairo to a version implementing recording surfaces"
#endif
    SurfacePtr surf
      (new Surface
       (cairo_recording_surface_create(CAIRO_CONTENT_COLOR_ALPHA,NULL)));
    for (auto& i: items)
      try 
        {
          i->draw(surf->cairo());  
          localZoom=i->zoomFactor;
        }
      catch (const std::exception& e) 
        {cerr<<"illegal exception caught in draw()"<<e.what()<<endl;}
      catch (...) {cerr<<"illegal exception caught in draw()";}
    cairo_recording_surface_ink_extents(surf->surface(),
                                        &x0,&y0,&x1,&y1);

    for (auto& i: groups)
      {
        float w=0.5f*i->width*i->zoomFactor,
          h=0.5f*i->height*i->zoomFactor;
        x0=min(i->x()-0.5*w, x0);
        x1=max(i->x()+0.5*w, x1);
        y0=min(i->y()-0.5*h, y0);
        y1=max(i->x()+0.5*h, y1);
      }


    // if there are no contents, result is not finite. In this case,
    // set the content bounds to a 10x10 sized box around the centroid of the I/O variables.

    if (x0==numeric_limits<float>::max())
      {
        // TODO!
//        float cx=0, cy=0;
//        for (int i: inVariables)
//          {
//            cx+=cminsky().variables[i]->x();
//            cy+=cminsky().variables[i]->y();
//          }
//        for (int i: outVariables)
//          {
//            cx+=cminsky().variables[i]->x();
//            cy+=cminsky().variables[i]->y();
//          }
//        int n=inVariables.size()+outVariables.size();
//        cx/=n;
//        cy/=n;
//        x0=cx-10;
//        x1=cx+10;
//        y0=cy-10;
//        y1=cy+10;
      }
    else
      {
        // extend width by 2 pixels to allow for the slightly oversized variable icons
        x0-=2*this->localZoom();
        y0-=2*this->localZoom();
        x1+=2*this->localZoom();
        y1+=2*this->localZoom();
      }

    return localZoom;
  }

  float Group::computeDisplayZoom()
  {
    double x0, x1, y0, y1;
    float l, r;
    float lz=contentBounds(x0,y0,x1,y1);
    x0=min(x0,double(x()));
    x1=max(x1,double(x()));
    y0=min(y0,double(y()));
    y1=max(y1,double(y()));
    // first compute the value assuming margins are of zero width
    displayZoom = 2*max( max(x1-x(), x()-x0)/width, max(y1-y(), y()-y0)/height );

    // account for shrinking margins
    float readjust=zoomFactor/edgeScale() / (displayZoom>1? displayZoom:1);
    //TODO    margins(l,r);
    l*=readjust; r*=readjust;
    displayZoom = max(displayZoom, 
                      float(max((x1-x())/(0.5f*width-r), (x()-x0)/(0.5f*width-l))));
  
    // displayZoom*=1.1*rotFactor()/lz;

    // displayZoom should never be less than 1
    displayZoom=max(displayZoom, 1.0f);
    return displayZoom;
  }

  const Group* Group::minimalEnclosingGroup(float x0, float y0, float x1, float y1) const
  {
    if (x0>x()-0.5*width || x1<x()+0.5*width || 
        y0>y()-0.5*height || y1<y()+0.5*height)
      return nullptr;
    // at this point, this is a candidate. Check if any child groups are also
    for (auto& g: groups)
      if (auto mg=g->minimalEnclosingGroup(x0,y0,x1,y1))
        return mg;
    return this;
  }

  void Group::setZoom(float factor)
  {
    zoomFactor=factor;
    computeDisplayZoom();
    float lzoom=localZoom();
    for (auto& i: items)
      i->zoomFactor=lzoom;
    for (auto& i: groups)
      i->setZoom(lzoom);
  }

}