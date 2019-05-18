# ------------------------------------------------------------------------------
# Numenta Platform for Intelligent Computing (NuPIC)
#
# Copyright (C) 2018-2019, David McDougall
#
# This program is free software: you can redistribute it and/or modify it under
# the terms of the GNU Affero Public License version 3 as published by the Free
# Software Foundation.
#
# This program is distributed in the hope that it will be useful, but WITHOUT
# ANY WARRANTY; without even the implied warranty of MERCHANTABILITY or FITNESS
# FOR A PARTICULAR PURPOSE. See the GNU Affero Public License for more details.
#
# You should have received a copy of the GNU Affero Public License along with
# this program.  If not, see http://www.gnu.org/licenses.
# ------------------------------------------------------------------------------
""" Swarming parameter search """

# TODO: I'd like to see a big summary of what this is doing, Ideally in the main
# lab report file.  It should include the min/mean/std/max of each value,
# velocity, and score across the swarm.

# TODO: Add notes to experiment summaries that they were created by swarming.

# TODO: Make CLI Arguments for these global constants: particle_strength, global_strength, velocity_strength
particle_strength   =  .25
global_strength     =  .50
velocity_strength   =  .95
assert(velocity_strength + particle_strength / 2 + global_strength / 2 >= 1)

import sys
import os
import random
import pickle

from nupic.optimization.parameter_set import ParameterSet
from nupic.optimization.optimizers import BaseOptimizer

class ParticleData:
    """
    Attributes:
        p.parameters - ParameterSet
        p.velocities - ParameterSet full of float
        p.best       - ParameterSet
        p.score      - float
        p.age        - Number of times this particle has been evaluated/updated.
    """
    def __init__(self, default_parameters):
        self.parameters = ParameterSet( default_parameters )
        self.best       = None
        self.best_score = None
        self.age        = 0
        self.initialize_velocities()

    def initialize_velocities(self):
        # Make a new parameter structure for the velocity data.
        self.velocities = ParameterSet( self.parameters )
        # Iterate through every field in the structure.
        for path in self.parameters.enumerate():
            value = self.parameters.get(path)
            max_percent_change = 10
            uniform = 2 * random.random() - 1
            if isinstance(value, float):
                velocity = value * uniform * (max_percent_change / 100.)
            elif isinstance(value, int):
                if abs(value) < 10:
                    velocity = uniform
                else:
                    velocity = value * uniform * (max_percent_change / 100.)
            else:
                raise NotImplementedError()
            self.velocities.apply( path, velocity )

    def update_position(self):
        for path in self.parameters.enumerate():
            position = self.parameters.get( path )
            velocity = self.velocities.get( path )
            self.parameters.apply( path, position + velocity )

    def update_velocity(self, global_best):
        for path in self.parameters.enumerate():
            postition     = self.parameters.get( path )
            velocity      = self.velocities.get( path )
            particle_best = self.best.get( path ) if self.best is not None else postition
            global_best_x = global_best.get( path ) if global_best is not None else postition

            # Update velocity.
            particle_bias = (particle_best - postition) * particle_strength * random.random()
            global_bias   = (global_best_x - postition)   * global_strength   * random.random()
            velocity = velocity * velocity_strength + particle_bias + global_bias
            self.velocities.apply( path, velocity )

    def update(self, score, global_best):
        self.age += 1
        self.update_position()
        self.update_velocity( global_best )
        if self.best is None or score > self.best_score:
            self.best       = self.parameters
            self.best_score = score
            print("New particle best score %g."%self.best_score)


class ParticleSwarmOptimization(BaseOptimizer):
    """
    Attributes:
        pso.lab           - Laboratory
        pso.particles     - Number of particles to use.
        pso.next_particle - Index into pso.swarm
        pso.swarm_path    - Data File for this particle swarm.
        pso.swarm         - List of ParticleData
        pso.best          - ParameterSet
        pso.best_score    - float
    """
    def add_arguments(parser):
        parser.add_argument('--swarming', type=int,
            help='Particle Swarm Optimization, number of particles to use.')

        parser.add_argument('--clear_scores', action='store_true',
            help=('Remove all scores from the particle swarm so that the '
                  'experiment can be safely altered.'))

    def use_this_optimizer(args):
        return args.swarming or args.clear_scores

    def __init__(self, lab, args):
        self.swarm_path    = os.path.join( lab.ae_directory, 'particle_swarm.pickle' )
        if args.clear_scores:
            self.clear_scores()
            sys.exit()
        # Setup the particle swarm.
        self.lab           = lab
        self.swarm         = []
        self.particles     = args.swarming
        self.next_particle = random.randrange( self.particles )
        self.best          = None
        self.best_score    = None
        assert( self.particles >= args.processes )
        # Try loading an existing particle swarm.
        try:
            self.load()
            if self.particles != len(self.swarm):
                print("Warning: requested number of particles does not match number stored on file.")
        except FileNotFoundError:
            pass
        # Add new particles as necessary.
        while len(self.swarm) < self.particles:
            if self.best is not None:
                new_particle = ParticleData( self.best )
            else:
                new_particle = ParticleData( self.lab.default_parameters )
            self.swarm.append( new_particle )
            # Evaluate the default parameters a few times, before branching out
            # to the more experimental stuff.
            if( len(self.swarm) > 3 ):
                new_particle.update_position()
                new_particle.update_position()

    def suggest_parameters(self):
        particle_data = self.swarm[self.next_particle]
        self.next_particle = (self.next_particle + 1) % self.particles
        particle_data.parameters.typecast( self.lab.structure )
        return particle_data.parameters

    def collect_results(self, parameters, score):
        for particle in self.swarm:
            if particle.parameters == parameters:
                break

        if isinstance(score, Exception):
            # Program crashed, replace this particle.
            if particle.best is not None:
                particle.parameters = ParameterSet( particle.best )
            elif self.best is not None:
                particle.parameters = ParameterSet( self.best )
            else:
                particle.parameters = ParameterSet( self.lab.default_parameters)
            particle.initialize_velocities()
            particle.update_position()
        else:
            # Update with results of this particles evaluation.
            particle.update( score, self.best )
            if self.best is None or score > self.best_score:
                self.best       = particle.parameters
                self.best_score = score
                print("New global best score %g."%score)
        self.save()

    def save(self):
        data = (self.swarm, self.best, self.best_score)
        with open(self.swarm_path, 'wb') as file:
            pickle.dump(data, file)

    def load(self):
        with open(self.swarm_path, 'rb') as file:
            data = pickle.load( file )
        self.swarm, self.best, self.best_score = data

    def clear_scores(self):
        try:
            self.load()
        except FileNotFoundError:
            print("Particle Swarm not initialized, nothing to do.")
        else:
            self.best_score = float('-inf')
            for entry in self.swarm:
                entry.best_score = float('-inf')
            self.save()
            print("Removed scores from Particle Swarm.")

