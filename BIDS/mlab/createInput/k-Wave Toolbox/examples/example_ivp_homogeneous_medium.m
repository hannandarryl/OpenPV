% Homogeneous Propagation Medium Example
%
% This example provides a simple demonstration of using k-Wave for the
% simulation and detection of the pressure field generated by an initial
% pressure distribution within a two-dimensional homogeneous propagation
% medium.
%
% author: Bradley Treeby
% date: 29th June 2009
% last update: 17th October 2011
%  
% This function is part of the k-Wave Toolbox (http://www.k-wave.org)
% Copyright (C) 2009-2012 Bradley Treeby and Ben Cox

% This file is part of k-Wave. k-Wave is free software: you can
% redistribute it and/or modify it under the terms of the GNU Lesser
% General Public License as published by the Free Software Foundation,
% either version 3 of the License, or (at your option) any later version.
% 
% k-Wave is distributed in the hope that it will be useful, but WITHOUT ANY
% WARRANTY; without even the implied warranty of MERCHANTABILITY or FITNESS
% FOR A PARTICULAR PURPOSE.  See the GNU Lesser General Public License for
% more details. 
% 
% You should have received a copy of the GNU Lesser General Public License
% along with k-Wave. If not, see <http://www.gnu.org/licenses/>. 

clear all;

% =========================================================================
% SIMULATION
% =========================================================================

% create the computational grid
Nx = 128;           % number of grid points in the x (row) direction
Ny = 128;           % number of grid points in the y (column) direction
dx = 0.1e-3;        % grid point spacing in the x direction [m]
dy = 0.1e-3;        % grid point spacing in the y direction [m]
kgrid = makeGrid(Nx, dx, Ny, dy);

% define the properties of the propagation medium
medium.sound_speed = 1500;  % [m/s]
medium.alpha_coeff = 0.75;  % [dB/(MHz^y cm)]
medium.alpha_power = 1.5; 

% create initial pressure distribution using makeDisc
disc_magnitude = 5; % [au]
disc_x_pos = 50;    % [grid points]
disc_y_pos = 50;    % [grid points]
disc_radius = 8;    % [grid points]
disc_1 = disc_magnitude*makeDisc(Nx, Ny, disc_x_pos, disc_y_pos, disc_radius);

disc_magnitude = 3; % [au]
disc_x_pos = 80;    % [grid points]
disc_y_pos = 60;    % [grid points]
disc_radius = 5;    % [grid points]
disc_2 = disc_magnitude*makeDisc(Nx, Ny, disc_x_pos, disc_y_pos, disc_radius);

source.p0 = disc_1 + disc_2;

% define a centered circular sensor
sensor_radius = 4e-3;   % [m]
num_sensor_points = 50;
sensor.mask = makeCartCircle(sensor_radius, num_sensor_points);

% run the simulation
sensor_data = kspaceFirstOrder2D(kgrid, medium, source, sensor);

% =========================================================================
% VISUALISATION
% =========================================================================

% plot the initial pressure and sensor distribution
figure;
imagesc(kgrid.y_vec*1e3, kgrid.x_vec*1e3, source.p0 + cart2grid(kgrid, sensor.mask), [-1 1]);
colormap(getColorMap);
ylabel('x-position [mm]');
xlabel('y-position [mm]');
axis image;

% plot the simulated sensor data
figure;
imagesc(sensor_data, [-1, 1]);
colormap(getColorMap);
ylabel('Sensor Position');
xlabel('Time Step');
colorbar;