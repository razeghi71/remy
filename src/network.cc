#include "network.hh"

#include "sendergangofgangs.cc"
#include "link-templates.cc"

template <class Gang1Type, class Gang2Type>
Network<Gang1Type, Gang2Type>::Network( const typename Gang1Type::Sender & example_sender1,
					const typename Gang2Type::Sender & example_sender2,
					PRNG & s_prng,
					const NetConfig & config )
  : _prng( s_prng ),
    _senders( Gang1Type( config.mean_on_duration, config.mean_off_duration, config.num_senders, example_sender1, _prng ),
	      Gang2Type( config.mean_on_duration, config.mean_off_duration, config.num_senders, example_sender2, _prng, config.num_senders ) ),
    _link( config.link_ppt ),
    _delay( config.delay ),
    _rec(),
    _tickno( 0 ),
    _history(),
    _max_history( 50000 ),
    _start_config()
{
}

template <class Gang1Type, class Gang2Type>
Network<Gang1Type, Gang2Type>::Network( const typename Gang1Type::Sender & example_sender1,
					PRNG & s_prng,
					const NetConfig & config )
  : _prng( s_prng ),
    _senders( Gang1Type( config.mean_on_duration, config.mean_off_duration, config.num_senders, example_sender1, _prng ),
	      Gang2Type() ),
    _link( config.link_ppt ),
    _delay( config.delay ),
    _rec(),
    _tickno( 0 ),
    _history(),
    _max_history( 10000 ),
    _start_config()
{
}

template <class Gang1Type, class Gang2Type>
void Network<Gang1Type, Gang2Type>::tick( void )
{
  _senders.tick( _link, _rec, _tickno );
  _link.tick( _delay, _tickno );
  _delay.tick( _rec, _tickno );

  /* save state for sender 1 */
  auto & mem = _senders.mutable_gang1().mutable_sender( 0 ).mutable_sender().current_memory();
  _history.emplace_back( mem.field( 0 ), mem.field( 1 ),
                         mem.field( 2 ), mem.field( 3 ),
                         _link.buffer_size(), _tickno );

  if( _history.size() >= _max_history ) {
    _history.pop_front();
  }

  for( int i = _history.size()-2; i >= 0; i-- ) {
    const auto &pt = _history.at( i );

    if( pt == _history.back() ) {
      printf("found match for %s\n \t%s in %f ticks\n" , pt.str().c_str(), 
             _history.back().str().c_str(),
             _tickno - pt._tickno);
      break;
    }
  }
}

template <class Gang1Type, class Gang2Type>
void Network<Gang1Type, Gang2Type>::run_simulation( const double & duration )
{
  assert( _tickno == 0 );

  while ( _tickno < duration ) {
    /* find element with soonest event */
    _tickno = min( min( _senders.next_event_time( _tickno ),
			_link.next_event_time( _tickno ) ),
		   min( _delay.next_event_time( _tickno ),
			_rec.next_event_time( _tickno ) ) );

    if ( _tickno > duration ) break;
    assert( _tickno < std::numeric_limits<double>::max() );

    tick();
  }
}

template <class Gang1Type, class Gang2Type>
void Network<Gang1Type, Gang2Type>::run_simulation_until( const double tick_limit )
{
  if ( _tickno >= tick_limit ) {
    return;
  }

  while ( true ) {
    /* find element with soonest event */
    double next_tickno = min( min( _senders.next_event_time( _tickno ),
				   _link.next_event_time( _tickno ) ),
			      min( _delay.next_event_time( _tickno ),
				   _rec.next_event_time( _tickno ) ) );

    if ( next_tickno > tick_limit ) {
      _tickno = tick_limit;
      break;
    }

    assert( next_tickno < std::numeric_limits<double>::max() );

    _tickno = next_tickno;

    tick();
  }
}

template <class Gang1Type, class Gang2Type>
void Network<Gang1Type, Gang2Type>::run_simulation_with_config( const double & tick_limit,
                                   const double & sewma, const double & rewma,
                                   const double & rttr,
                                   const double & slow_rewma,
                                   const unsigned int buffer_size __attribute((unused)) )
{
  auto & sender_1 = _senders.mutable_gang1().mutable_sender( 0 ).mutable_sender();
  sender_1.set_mem( std::vector< double > { sewma, rewma, rttr, slow_rewma } );

  if ( _tickno >= tick_limit ) {
    return;
  }

  /*for( unsigned int i = 0; i < buffer_size; i++ ) {
    sender_1.force_send( 0, _link, 0 );
    }*/

  while ( true ) {
    /* find element with soonest event */
    double next_tickno = min( min( _senders.next_event_time( _tickno ),
				   _link.next_event_time( _tickno ) ),
			      min( _delay.next_event_time( _tickno ),
				   _rec.next_event_time( _tickno ) ) );

    if ( next_tickno > tick_limit ) {
      _tickno = tick_limit;
      break;
    }

    assert( next_tickno < std::numeric_limits<double>::max() );
    _tickno = next_tickno;

    tick();
  }
}

template <class Gang1Type, class Gang2Type>
vector<unsigned int> Network<Gang1Type, Gang2Type>::packets_in_flight( void ) const
{
  unsigned int num_senders = _senders.count_senders();
  vector<unsigned int> ret = _link.packets_in_flight( num_senders );
  const vector<unsigned int> delayed = _delay.packets_in_flight( num_senders );

  for ( unsigned int i = 0; i < num_senders; i++ ) {
    ret.at( i ) += delayed.at( i );
  }

  return ret;
}
