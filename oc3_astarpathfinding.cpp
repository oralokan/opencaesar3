#include "oc3_astarpathfinding.hpp"
#include "oc3_tilemap.hpp"
#include "oc3_positioni.hpp"
#include "oc3_astarpoint.hpp"
#include "oc3_path_finding.hpp"

using namespace std;

const AStarPoint Pathfinder::invalidPoint = AStarPoint( 0 ); 

Pathfinder::Pathfinder()
{
}

void Pathfinder::update( const Tilemap& tilemap )
{
  _tilemap = const_cast< Tilemap* >( &tilemap );

  for( int i=0; i < tilemap.getSize(); i++ )
  {
    for( int j=0; j < tilemap.getSize(); j++ )
    {
      TilePos pos( i, j );
      if ( !pointExists( pos ) )
      {
        grid[i][j] = new AStarPoint( &tilemap.at( pos ) );
      }
    }
  }
}

bool Pathfinder::getPath( const TilePos& start, const TilePos& stop,  
                          PathWay& oPathWay, bool checkLast, bool tryTraverse,
                          const Size& arrivedArea )
{
  if( pointIsWalkable( start ) )
  {
    // Calculate line direction
    int const dx = (start.getI() < stop.getI()) ? 1 : -1;
    int const dy = (start.getJ() < stop.getJ()) ? 1 : -1;

    bool pathIsWalkable = tryTraverse;

    list<AStarPoint*> points1;    
    if( tryTraverse )
    {
      // Get traversing points      
      points1 = getTraversingPoints( start, stop );
      list<AStarPoint*>::iterator lastStepIt = checkLast ? points1.end() : points1.rbegin().base();

      // Check if the direct path is safe
      for( list<AStarPoint*>::iterator i = points1.begin(); 
           i != lastStepIt && pathIsWalkable; ++i )
      {
        pathIsWalkable = (*i)->isWalkable();
      }
    }

    if( pathIsWalkable )
    {
      oPathWay.init( *_tilemap, _tilemap->at( start ) );
      list<AStarPoint*>::iterator i = points1.begin();
      i++;
      for( ; i != points1.end() && pathIsWalkable; ++i )
      {
        oPathWay.setNextTile( *( (*i)->tile ) );
      }
      return true;
    }
    else
    {
      return aStar( start, stop, arrivedArea, oPathWay );
    }
  }
  else
  {
    cout << "not walkable" << endl;
  }

  return false;
}

std::list<AStarPoint*> Pathfinder::getTraversingPoints( const TilePos& start, const TilePos& stop )
{
  std::list<AStarPoint*> points;

  if( start == stop )
    return points;

  points.push_back( getPoint( start ) );

  TilePos housePos = stop;               
  while( points.back()->getPos() != housePos )
  {
    TilePos ij = points.back()->getPos();
    TilePos move( math::clamp( housePos.getI() - ij.getI(), -1, 1 ), math::clamp( housePos.getJ() - ij.getJ(), -1, 1 ) );
    points.push_back( getPoint( ij + move ) );
  }

  return points;
}

int Pathfinder::getMaxLoopCount() const
{
  return 300;
}

bool Pathfinder::aStar( const TilePos& startPos, const TilePos& stopPos, const Size& arrivedArea, PathWay& oPathWay )
{
  oPathWay.init( *_tilemap, _tilemap->at( startPos ) );

  // Define points to work with
  AStarPoint* start = getPoint( startPos );
  AStarPoint* end = getPoint( stopPos );
  AStarPoint* current;
  AStarPoint* child;

  // Define the open and the close list
  list<AStarPoint*> openList;
  list<AStarPoint*> closedList;
  list<AStarPoint*>::iterator i;

  map<AStarPoint*,AStarPoint::WalkableType> saveArrivedArea;
  for( int i=stopPos.getI(); i < stopPos.getI()+arrivedArea.getWidth(); i++ )
  {
    for( int j=stopPos.getJ(); j < stopPos.getJ() + arrivedArea.getHeight(); j++ )
    {
      AStarPoint* ap = getPoint( TilePos( i, j) );
      if( ap )
      {
        saveArrivedArea[ ap ] = ap->priorWalkable;
        ap->priorWalkable = AStarPoint::alwaysWalkable;
      }
    }
  }

  unsigned int n = 0;

  // Add the start point to the openList
  openList.push_back(start);
  start->opened = true;

  while (n == 0 || (current != end && n < getMaxLoopCount() ))
  {
    // Look for the smallest F value in the openList and make it the current point
    for (i = openList.begin(); i != openList.end(); ++ i)
    {
      if (i == openList.begin() || (*i)->getFScore() <= current->getFScore())
      {
        current = (*i);
      }
    }

    // Stop if we reached the end
    if( current == end ) 
    {
      break;
    }

    // Remove the current point from the openList
    openList.remove(current);
    current->opened = false;

    // Add the current point to the closedList
    closedList.push_back(current);
    current->closed = true;

    // Get all current's adjacent walkable points
    for (int x = -1; x < 2; x ++)
    {
      for (int y = -1; y < 2; y ++)
      {
        // If it's current point then pass
        if (x == 0 && y == 0)
        {
          continue;
        }

        // Get this point
        child = getPoint( current->getPos() + TilePos( x, y ) );

        // If it's closed or not walkable then pass
        if (child->closed || !child->isWalkable() )
        {
          continue;
        }

        // If we are at a corner
        if (x != 0 && y != 0)
        {
          // if the next horizontal point is not walkable or in the closed list then pass
          //AStarPoint* tmpPoint = getPoint( current->pos + TilePos( 0, y ) );
          TilePos tmp = current->getPos() + TilePos( 0, y );
          if( !pointIsWalkable( tmp ) || getPoint( tmp )->closed)
          {
            continue;
          }

          tmp = current->getPos() + TilePos( x, 0 );
          // if the next vertical point is not walkable or in the closed list then pass
          if( !pointIsWalkable( tmp ) || getPoint( tmp )->closed)
          {
            continue;
          }
        }

        // If it's already in the openList
        if (child->opened)
        {
          // If it has a wroste g score than the one that pass through the current point
          // then its path is improved when it's parent is the current point
          if (child->getGScore() > child->getGScore(current))
          {
            // Change its parent and g score
            child->setParent(current);
            child->computeScores(end);
          }
        }
        else
        {
          // Add it to the openList with current point as parent
          openList.push_back(child);
          child->opened = true;

          // Compute it's g, h and f score
          child->setParent(current);
          child->computeScores(end);
        }
      }
    }

    n++;
  }

  // Reset
  for (i = openList.begin(); i != openList.end(); ++ i)
  {
    (*i)->opened = false;
  }
  for (i = closedList.begin(); i != closedList.end(); ++ i)
  {
    (*i)->closed = false;
  }

  for( map<AStarPoint*,AStarPoint::WalkableType>::iterator it=saveArrivedArea.begin(); it != saveArrivedArea.end(); it++ )
  {
    it->first->priorWalkable = it->second;
  }

  if( n == getMaxLoopCount() )
  {
    return false;
  }
  // Resolve the path starting from the end point
  list<AStarPoint*> lPath;
  while( current->hasParent() && current != start )
  {
    lPath.push_front( current );
    current = current->getParent();
    n++;
  }

  for( list<AStarPoint*>::iterator lpIt=lPath.begin(); lpIt != lPath.end(); lpIt++ )
  {
    oPathWay.setNextTile( _tilemap->at( (*lpIt)->getPos() ) );
  }

  return oPathWay.getLength() > 1;
}

AStarPoint* Pathfinder::getPoint( const TilePos& pos )
{
  if( pointExists( pos ))
  {
    return grid[pos.getI()][pos.getJ()];
  }
  else
  {
    cout << "ERROR: failed to gather point " << pos.getI() << "x" << pos.getJ() << " on grid" << endl;
    return const_cast< AStarPoint* >( &invalidPoint );
  }
}

bool Pathfinder::pointExists( const TilePos& pos )
{
  return ( pos.getI() < grid.size() && pos.getJ() < grid[pos.getI()].size() );
}

bool Pathfinder::pointIsWalkable( const TilePos& pos )
{
  return (pointExists( pos ) && grid[ pos.getI() ][ pos.getJ() ]->isWalkable() );
}

Pathfinder& Pathfinder::getInstance()
{
  static Pathfinder inst;
  return inst;
}